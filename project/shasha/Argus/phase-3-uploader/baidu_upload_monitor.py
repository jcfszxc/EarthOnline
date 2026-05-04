#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
百度网盘自动上传监控程序
监控目录，自动上传文件到百度网盘，并在一定时间后删除本地文件
同时记录每场直播信息到 live_history.json
"""

import os
import re
import json
import time
import subprocess
import logging
from datetime import datetime, timedelta
from pathlib import Path
from typing import Dict, List, Optional

# ==================== 配置区域 ====================
# 监控目录
WATCH_DIR = os.path.expanduser("/root/Argus/phase-2-recorder/data/recordings/")
# BaiduPCS-Go 可执行文件路径
BAIDU_PCS_GO = "/root/Argus/phase-3-uploader/data/software/BaiduPCS-Go-v4.0.1-linux-amd64/BaiduPCS-Go"
# 百度网盘目标目录
BAIDU_TARGET_DIR = "/耗耗"
# 状态记录文件
STATE_FILE = "./data/upload_state.json"
# 统计报告文件
REPORT_FILE = "./data/upload_report.txt"
# 日志文件
LOG_FILE = "./data/upload_monitor.log"
# 直播历史记录文件
LIVE_HISTORY_FILE = "./data/live_history.json"

# 时间配置（分钟）
UPLOAD_DELAY = 5  # 文件创建后多久上传
DELETE_DELAY = 5  # 上传成功后多久删除
SCAN_INTERVAL = 5  # 扫描间隔

# 配置区新增（秒）：
FILE_STABLE_CHECK_INTERVAL = 10   # 两次大小采样间隔
FILE_STABLE_MIN_IDLE = 30         # 文件大小连续多少秒不变才认为录制结束

# ==================== 修复：record_live_history 加视频文件白名单 ====================
# 在配置区域添加：
# 视频主文件扩展名（稳定性检测的基准）
VIDEO_EXTENSIONS = {'.flv', '.mp4', '.ts', '.mkv', '.mov', '.avi', '.m4v'}
# 伴生文件扩展名（跟随主文件状态，不单独做稳定性检测）
COMPANION_EXTENSIONS = {'.xml'}


def get_primary_file(filepath: str) -> Optional[str]:
    """
    对于伴生文件（xml 等），返回同名的视频主文件路径。
    对于视频文件本身，返回自身。
    如果是伴生文件但找不到对应主文件，返回 None（等待主文件出现）。
    """
    ext = os.path.splitext(filepath)[1].lower()
    if ext not in COMPANION_EXTENSIONS:
        return filepath  # 本身就是主文件

    base = os.path.splitext(filepath)[0]
    for video_ext in VIDEO_EXTENSIONS:
        candidate = base + video_ext
        if os.path.exists(candidate):
            return candidate
    return None  # 主文件尚不存在


# ==================== 直播信息配置 ====================
# 直播间 URL（固定值）
LIVE_URL = "https://live.bilibili.com/31218790"
# 默认直播标题（当无法从文件名解析时使用）
DEFAULT_LIVE_TITLE = "最强大的女鹅"

# 文件名解析规则（正则表达式）
# 支持以下常见命名格式，按优先级匹配：
#
# 格式1: streamlink/yt-dlp 风格 — "Title_20260326_080412.flv"
#   捕获组: title, date(YYYYMMDD), time(HHMMSS)
FILENAME_PATTERN_1 = re.compile(
    r'^(?P<title>.+?)_(?P<date>\d{8})_(?P<time>\d{6})(?:\.\w+)?$'
)
# 格式2: 纯时间戳前缀 — "20260326_080412_耗耗直播.flv" 或 "20260326_080412.flv"
#   捕获组: date(YYYYMMDD), time(HHMMSS), title(可选)
FILENAME_PATTERN_2 = re.compile(
    r'^(?P<date>\d{8})_(?P<time>\d{6})(?:_(?P<title>.+?))?(?:\.\w+)?$'
)
# 格式3: 带连字符日期 — "2026-03-26_08-04-12.flv"
FILENAME_PATTERN_3 = re.compile(
    r'^(?P<date>\d{4}-\d{2}-\d{2})_(?P<time>\d{2}-\d{2}-\d{2})(?:\.\w+)?$'
)
# 格式4: 上面都不匹配时，回退到文件 mtime 推断日期和开始时间

# 格式5: 录播姬风格 — "录制-26279074-20260329-215834-737-day1[海豹]要抱抱.....flv"
#   提取最后一个 "-数字-" 之后的内容作为 title
FILENAME_PATTERN_BLREC = re.compile(
    r'^录制-\d+-\d{8}-\d{6}-\d+-(?P<title>.+?)(?:\.\w+)?$'
)


# ==================== 日志配置 ====================

from logging.handlers import RotatingFileHandler

# ==================== 日志配置（替换原有 basicConfig）====================
# 单个日志文件最大 5MB，保留最近 3 份，超出自动轮转
LOG_MAX_BYTES = 5 * 1024 * 1024  # 5 MB
LOG_BACKUP_COUNT = 3

logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

_file_handler = RotatingFileHandler(
    LOG_FILE, maxBytes=LOG_MAX_BYTES, backupCount=LOG_BACKUP_COUNT, encoding='utf-8'
)
_file_handler.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))

_console_handler = logging.StreamHandler()
_console_handler.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))

logger.addHandler(_file_handler)
logger.addHandler(_console_handler)


class FileState:
    """文件状态类"""
    PENDING = "pending"      # 待上传
    UPLOADED = "uploaded"    # 已上传
    DELETED = "deleted"      # 已删除


# ==================== 直播信息解析 ====================

def format_duration(seconds: int) -> str:
    """将秒数格式化为 'XhYm' 字符串，例如 15072 -> '4h11m'"""
    hours = seconds // 3600
    minutes = (seconds % 3600) // 60
    if hours > 0:
        return f"{hours}h{minutes:02d}m"
    return f"{minutes}m"


def get_video_duration_seconds(filepath: str) -> Optional[int]:
    """
    用 ffprobe 读取视频时长（秒）。
    ffprobe 不可用时返回 None，调用方会回退到文件时间戳差值。
    """
    try:
        result = subprocess.run(
            [
                "ffprobe", "-v", "error",
                "-show_entries", "format=duration",
                "-of", "default=noprint_wrappers=1:nokey=1",
                filepath
            ],
            capture_output=True, text=True, timeout=30
        )
        if result.returncode == 0:
            duration_str = result.stdout.strip()
            if duration_str and duration_str != "N/A":
                return int(float(duration_str))
    except (FileNotFoundError, subprocess.TimeoutExpired, ValueError):
        pass
    return None



# ==================== 改动2：parse_live_info_from_file 中加入新模式匹配 ====================
# 在现有三种 pattern 循环之前，优先尝试录播姬格式：

def parse_live_info_from_file(filepath: str) -> dict:
    filename = os.path.splitext(os.path.basename(filepath))[0]
    stat = os.stat(filepath)
    mtime = datetime.fromtimestamp(stat.st_mtime)

    date_str: Optional[str] = None
    start_str: Optional[str] = None
    title: str = DEFAULT_LIVE_TITLE

    # --- 优先尝试录播姬格式 ---
    m = FILENAME_PATTERN_BLREC.match(filename)
    if m:
        parsed_title = m.group("title")
        if parsed_title:
            title = parsed_title
        # 从文件名中提取日期和时间（录播姬格式固定位置）
        parts = filename.split("-")
        # 格式: 录制 / roomid / YYYYMMDD / HHMMSS / index / title
        if len(parts) >= 6:
            raw_date = parts[2]   # '20260329'
            raw_time = parts[3]   # '215834'
            if len(raw_date) == 8:
                date_str = f"{raw_date[:4]}-{raw_date[4:6]}-{raw_date[6:]}"
            if len(raw_time) == 6:
                start_str = f"{raw_time[:2]}:{raw_time[2:4]}"
    else:
        # --- 原有三种通用正则 ---
        for pattern in (FILENAME_PATTERN_1, FILENAME_PATTERN_2, FILENAME_PATTERN_3):
            m = pattern.match(filename)
            if not m:
                continue
            gd = m.groupdict()
            raw_date = gd.get("date", "").replace("-", "")
            raw_time = gd.get("time", "").replace("-", "")
            if len(raw_date) == 8:
                date_str = f"{raw_date[:4]}-{raw_date[4:6]}-{raw_date[6:]}"
            if len(raw_time) == 6:
                start_str = f"{raw_time[:2]}:{raw_time[2:4]}"
            parsed_title = gd.get("title")
            if parsed_title:
                title = parsed_title
            break

    # --- 回退：用 mtime ---
    if date_str is None:
        date_str = mtime.strftime("%Y-%m-%d")
    if start_str is None:
        start_str = mtime.strftime("%H:%M")

    # --- 计算时长（此时文件已录制完成，ffprobe 结果可信）---
    duration_seconds = get_video_duration_seconds(filepath)
    if duration_seconds is not None:
        duration_str = format_duration(duration_seconds)
    else:
        try:
            start_dt = datetime.strptime(f"{date_str} {start_str}", "%Y-%m-%d %H:%M")
            diff = int((mtime - start_dt).total_seconds())
            duration_str = format_duration(max(diff, 0)) if diff > 0 else "0m"
        except ValueError:
            duration_str = "0m"

    return {
        "date": date_str,
        "title": title,
        "start": start_str,
        "duration": duration_str,
        "url": LIVE_URL,
        "tags": []
    }



# ==================== 直播历史记录管理 ====================

class LiveHistoryManager:
    """管理 live_history.json 的读写，防止重复追加"""

    def __init__(self, history_file: str):
        self.history_file = history_file
        self._ensure_dir()

    def _ensure_dir(self):
        os.makedirs(os.path.dirname(self.history_file), exist_ok=True)

    def load(self) -> List[dict]:
        if not os.path.exists(self.history_file):
            return []
        try:
            with open(self.history_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
                return data if isinstance(data, list) else []
        except Exception as e:
            logger.error(f"读取 live_history.json 失败: {e}")
            return []

    def save(self, records: List[dict]):
        try:
            with open(self.history_file, 'w', encoding='utf-8') as f:
                json.dump(records, f, ensure_ascii=False, indent=2)
        except Exception as e:
            logger.error(f"写入 live_history.json 失败: {e}")

    def append_if_new(self, live_info: dict, source_filepath: str) -> bool:
        """
        追加一条直播记录（如果尚未记录过）。
        去重依据：date + start 组合，避免同一场直播被重复写入。
        返回 True 表示成功追加了新记录。
        """
        records = self.load()

        # 去重检查
        for existing in records:
            if existing.get("date") == live_info["date"] and \
               existing.get("start") == live_info["start"]:
                logger.debug(
                    f"直播记录已存在，跳过: {live_info['date']} {live_info['start']}"
                )
                return False

        records.append(live_info)
        # 按日期+开始时间排序，保持文件整洁
        records.sort(key=lambda r: (r.get("date", ""), r.get("start", "")))
        self.save(records)
        logger.info(
            f"新增直播记录: {live_info['date']} {live_info['start']} "
            f"时长={live_info['duration']} 来源={os.path.basename(source_filepath)}"
        )
        return True


# ==================== 上传监控器 ====================

class UploadMonitor:
    """上传监控器"""

    def __init__(self):
        self.state_data = self.load_state()
        self.live_history = LiveHistoryManager(LIVE_HISTORY_FILE)

    def load_state(self) -> Dict:
        """加载状态数据"""
        if os.path.exists(STATE_FILE):
            try:
                with open(STATE_FILE, 'r', encoding='utf-8') as f:
                    return json.load(f)
            except Exception as e:
                logger.error(f"加载状态文件失败: {e}")
                return {}
        return {}

    def save_state(self):
        """保存状态数据"""
        try:
            os.makedirs(os.path.dirname(STATE_FILE), exist_ok=True)
            with open(STATE_FILE, 'w', encoding='utf-8') as f:
                json.dump(self.state_data, f, ensure_ascii=False, indent=2)
        except Exception as e:
            logger.error(f"保存状态文件失败: {e}")

    def get_file_create_time(self, filepath: str) -> datetime:
        """获取文件修改时间（Linux 下作为创建时间使用）"""
        stat = os.stat(filepath)
        return datetime.fromtimestamp(stat.st_mtime)

    def scan_directory(self) -> List[str]:
        """扫描目录获取所有文件"""
        files = []
        try:
            for root, dirs, filenames in os.walk(WATCH_DIR):
                for filename in filenames:
                    if filename.endswith(('.json', '.txt', '.log')):
                        continue
                    filepath = os.path.join(root, filename)
                    files.append(filepath)
        except Exception as e:
            logger.error(f"扫描目录失败: {e}")
        return files

    def upload_to_baidu(self, filepath: str) -> bool:
        """上传文件到百度网盘"""
        try:
            rel_path = os.path.relpath(os.path.dirname(filepath), WATCH_DIR)
            target_dir = BAIDU_TARGET_DIR if rel_path == '.' else \
                os.path.join(BAIDU_TARGET_DIR, rel_path)

            logger.info(f"开始上传: {filepath} -> {target_dir}")
            cmd = [BAIDU_PCS_GO, 'upload', filepath, target_dir]
            result = subprocess.run(
                cmd, capture_output=True, text=True, timeout=600
            )

            if result.returncode == 0:
                logger.info(f"上传成功: {filepath}")
                return True
            else:
                logger.error(f"上传失败: {filepath}, 错误: {result.stderr}")
                return False

        except subprocess.TimeoutExpired:
            logger.error(f"上传超时: {filepath}")
            return False
        except Exception as e:
            logger.error(f"上传异常: {filepath}, 错误: {e}")
            return False

    def delete_local_file(self, filepath: str) -> bool:
        """删除本地文件"""
        try:
            if os.path.exists(filepath):
                os.remove(filepath)
                logger.info(f"删除本地文件: {filepath}")
                return True
            else:
                logger.warning(f"文件不存在: {filepath}")
                return False
        except Exception as e:
            logger.error(f"删除文件失败: {filepath}, 错误: {e}")
            return False

    def record_live_history(self, filepath: str):
        """解析文件信息并写入直播历史记录（仅限视频文件）"""
        ext = os.path.splitext(filepath)[1].lower()
        if ext not in VIDEO_EXTENSIONS:
            logger.debug(f"跳过非视频文件的直播历史记录: {filepath}")
            return
        try:
            live_info = parse_live_info_from_file(filepath)
            self.live_history.append_if_new(live_info, filepath)
        except Exception as e:
            logger.error(f"记录直播历史失败: {filepath}, 错误: {e}")



# ==================== 改动2：process_files 中统一用主文件稳定性控制上传时机 ====================

    def process_files(self):
        """处理文件的主逻辑"""
        now = datetime.now()
        files = self.scan_directory()
        logger.info(f"扫描到 {len(files)} 个文件")

        for filepath in files:
            # 初始化文件状态
            if filepath not in self.state_data:
                create_time = self.get_file_create_time(filepath)
                self.state_data[filepath] = {
                    'status': FileState.PENDING,
                    'create_time': create_time.isoformat(),
                    'upload_time': None,
                    'delete_time': None,
                    'file_size': os.path.getsize(filepath),
                    'live_recorded': False
                }
                logger.info(f"发现新文件: {filepath}")

            file_info = self.state_data[filepath]
            create_time = datetime.fromisoformat(file_info['create_time'])

            # 处理待上传文件
            if file_info['status'] == FileState.PENDING:
                if now - create_time >= timedelta(minutes=UPLOAD_DELAY):

                    # ★ 找到控制上传时机的主文件
                    primary = get_primary_file(filepath)

                    if primary is None:
                        # 伴生文件的主文件还不存在，等待
                        logger.info(f"等待主文件出现，跳过: {filepath}")
                        continue

                    # ★ 对主文件做稳定性检测（伴生文件复用主文件结论）
                    if not self.is_file_stable(primary):
                        logger.info(f"主文件仍在写入中，跳过本轮: {filepath}")
                        continue

                    logger.info(f"文件满足上传条件: {filepath} (创建于 {create_time})")
                    if self.upload_to_baidu(filepath):
                        file_info['status'] = FileState.UPLOADED
                        file_info['upload_time'] = now.isoformat()

                        # 直播历史只由视频主文件触发
                        ext = os.path.splitext(filepath)[1].lower()
                        if ext in VIDEO_EXTENSIONS and not file_info.get('live_recorded', False):
                            self.record_live_history(filepath)
                            file_info['live_recorded'] = True

                        self.save_state()

            # 处理已上传文件（删除逻辑不变）
            elif file_info['status'] == FileState.UPLOADED:
                upload_time = datetime.fromisoformat(file_info['upload_time'])
                if now - upload_time >= timedelta(minutes=DELETE_DELAY):
                    logger.info(f"文件满足删除条件: {filepath} (上传于 {upload_time})")
                    if self.delete_local_file(filepath):
                        file_info['status'] = FileState.DELETED
                        file_info['delete_time'] = now.isoformat()
                        self.save_state()

    def generate_report(self):
        """生成统计报告"""
        pending, uploaded, deleted = [], [], []

        for filepath, info in self.state_data.items():
            status = info['status']
            if status == FileState.PENDING:
                pending.append((filepath, info))
            elif status == FileState.UPLOADED:
                uploaded.append((filepath, info))
            elif status == FileState.DELETED:
                deleted.append((filepath, info))

        # 读取直播历史条目数
        live_count = len(self.live_history.load())

        report_lines = [
            "=" * 80,
            "百度网盘上传监控报告",
            f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            "=" * 80,
            "",
            "📊 统计摘要:",
            f"  - 待上传文件: {len(pending)} 个",
            f"  - 已上传文件: {len(uploaded)} 个",
            f"  - 已删除文件: {len(deleted)} 个",
            f"  - 总计: {len(self.state_data)} 个",
            f"  - 直播历史记录: {live_count} 条",
            "",
        ]

        def file_section(label, items, sort_key, reverse=False, limit=None):
            lines = [label, "-" * 80]
            sorted_items = sorted(items, key=lambda x: x[1][sort_key], reverse=reverse)
            shown = sorted_items[:limit] if limit else sorted_items
            for fp, info in shown:
                size_mb = info['file_size'] / (1024 * 1024)
                recorded = "✅" if info.get('live_recorded') else "—"
                lines.append(
                    f"  📄 {os.path.basename(fp)}\n"
                    f"     路径: {fp}\n"
                    f"     大小: {size_mb:.2f} MB  直播已记录: {recorded}\n"
                )
            if limit and len(items) > limit:
                lines.append(f"  ... 以及其他 {len(items) - limit} 个文件\n")
            lines.append("")
            return lines

        if pending:
            report_lines += file_section(
                "📁 待上传文件:", pending, 'create_time'
            )
        if uploaded:
            report_lines += file_section(
                "✅ 已上传文件:", uploaded, 'upload_time', reverse=True, limit=10
            )
        if deleted:
            report_lines += file_section(
                "🗑️  已删除文件 (最近10个):", deleted, 'delete_time', reverse=True, limit=10
            )

        report_lines.append("=" * 80)

        try:
            os.makedirs(os.path.dirname(REPORT_FILE), exist_ok=True)
            with open(REPORT_FILE, 'w', encoding='utf-8') as f:
                f.write('\n'.join(report_lines))
            logger.info(f"统计报告已生成: {REPORT_FILE}")
        except Exception as e:
            logger.error(f"生成报告失败: {e}")

    def run(self):
        """运行监控程序"""
        logger.info("=" * 60)
        logger.info("百度网盘上传监控程序启动")
        logger.info(f"监控目录: {WATCH_DIR}")
        logger.info(f"上传延迟: {UPLOAD_DELAY} 分钟")
        logger.info(f"删除延迟: {DELETE_DELAY} 分钟")
        logger.info(f"扫描间隔: {SCAN_INTERVAL} 分钟")
        logger.info(f"直播历史: {LIVE_HISTORY_FILE}")
        logger.info("=" * 60)

        try:
            while True:
                logger.info("开始新一轮扫描...")
                self.process_files()
                self.generate_report()
                logger.info(f"扫描完成，等待 {SCAN_INTERVAL} 分钟后进行下次扫描...")
                time.sleep(SCAN_INTERVAL * 60)
        except KeyboardInterrupt:
            logger.info("接收到停止信号，程序退出")
            self.save_state()
        except Exception as e:
            logger.error(f"程序异常: {e}", exc_info=True)
            self.save_state()

    def is_file_stable(self, filepath: str) -> bool:
        """
        检测文件是否已停止写入（录制结束）。
        采样两次文件大小，间隔 FILE_STABLE_CHECK_INTERVAL 秒，
        若大小不变且距最后修改超过 FILE_STABLE_MIN_IDLE 秒则认为稳定。
        """
        try:
            size1 = os.path.getsize(filepath)
            mtime1 = os.path.getmtime(filepath)
            time.sleep(FILE_STABLE_CHECK_INTERVAL)
            size2 = os.path.getsize(filepath)
            mtime2 = os.path.getmtime(filepath)

            if size1 != size2 or mtime1 != mtime2:
                return False  # 还在写入

            idle_seconds = time.time() - mtime2
            return idle_seconds >= FILE_STABLE_MIN_IDLE
        except FileNotFoundError:
            return False
        

def main():
    """主函数"""
    if not os.path.exists(BAIDU_PCS_GO):
        logger.error(f"BaiduPCS-Go 不存在: {BAIDU_PCS_GO}")
        return
    if not os.path.exists(WATCH_DIR):
        logger.error(f"监控目录不存在: {WATCH_DIR}")
        return

    monitor = UploadMonitor()
    monitor.run()


if __name__ == "__main__":
    main()