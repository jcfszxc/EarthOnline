import sys
import os
import re
import random
import math
import configparser
from datetime import datetime, date

from PyQt5.QtWidgets import (
    QApplication, QLabel, QMenu, QAction, QSystemTrayIcon,
    QWidget, QPushButton, QDialog, QSpinBox,
    QFormLayout, QDialogButtonBox
)
from PyQt5.QtCore import Qt, QTimer, QPoint, QSize, QRectF
from PyQt5.QtGui import (
    QPixmap, QIcon, QPainter, QColor, QPen,
    QFont, QPainterPath, QBrush, QLinearGradient
)

# ─────────────────────────── paths ───────────────────────────
BASE_DIR        = os.path.dirname(os.path.abspath(__file__))
DATA_DIR        = os.path.join(BASE_DIR, "data")
ICON_DIR        = os.path.join(DATA_DIR, "icon")
PET_DIR         = os.path.join(DATA_DIR, "shasha")
CONFIG_INI      = os.path.join(PET_DIR,  "petconfig.ini")
DIALOGUES_TOML  = os.path.join(PET_DIR,  "dialogues.toml")
KNOWN_WORDS_TXT = os.path.join(PET_DIR,  "known_words.txt")
TRAY_PNG        = os.path.join(DATA_DIR, "tray.png")

def icon(name): return os.path.join(ICON_DIR, name)
def pet(name):  return os.path.join(PET_DIR,  name)

# ─────────────────────────── data loading ────────────────────
def _load_dialogues() -> dict:
    try:
        import tomllib
    except ImportError:
        try:
            import tomli as tomllib
        except ImportError:
            raise RuntimeError("需要 tomllib (Python 3.11+) 或 tomli (pip install tomli)")
    with open(DIALOGUES_TOML, "rb") as f:
        return tomllib.load(f)["dialogues"]

def _load_known_words() -> set:
    with open(KNOWN_WORDS_TXT, encoding="utf-8") as f:
        return {l.strip().lower() for l in f if l.strip() and not l.startswith("#")}

DIALOGUES    = _load_dialogues()
_KNOWN_WORDS = _load_known_words()

from games import RPSGame, BombGame, SlotsGame

# ─────────────────────────── constants ───────────────────────
AFFECTION_LEVELS = [
    (0,    "陌生人",  "#aaaaaa"),
    (200,  "朋友",    "#66bb6a"),
    (600,  "好朋友",  "#42a5f5"),
    (1400, "挚友",    "#ab47bc"),
    (2500, "真命天选","#ffa726"),
]

DAILY_LIMITS = dict(eat=5, drag=3, click=3, sleep=1)

AFFECTION_DELTA = dict(eat=3, drag=2, click=2, sleep=1, idle_decay=-1)

STREAK_REWARDS = {
    2:  (5,  "streak_2"),
    3:  (8,  "streak_3"),
    7:  (20, "streak_7"),
    14: (35, "streak_14"),
    30: (60, "streak_30"),
}

IDLE_DECAY_INTERVAL = 60
FRAME_COUNTS = dict(walkleft=6, eat=47, sleep=15, drag=6, falling=6, hide=6)

def affection_level(val):
    level = AFFECTION_LEVELS[0]
    for t in AFFECTION_LEVELS:
        if val >= t[0]: level = t
    return level

def next_level_threshold(val):
    for threshold, _, _ in AFFECTION_LEVELS:
        if threshold > val: return threshold
    return None

# ─────────────────────────── config ──────────────────────────
class Config:
    PET_DEFAULTS = dict(speed=3, scale=1, interval_walk=8,
                        interval_eat=15, interval_sleep=20,
                        affection=0, coins=0)
    DAILY_DEFAULTS = dict(
        last_date="", eat_count=0, drag_count=0,
        click_count=0, sleep_count=0,
        streak=0, last_streak_date=""
    )

    def __init__(self):
        self.cfg = configparser.ConfigParser()
        self.cfg.read(CONFIG_INI, encoding="utf-8-sig")
        for sec in ("pet", "daily"):
            if not self.cfg.has_section(sec):
                self.cfg.add_section(sec)

    def _get(self, sec, key, default):
        try:
            val = self.cfg.get(sec, key)
            return float(val) if '.' in val else int(val)
        except: return default

    def _set(self, sec, key, val):
        self.cfg.set(sec, key, str(val))
        with open(CONFIG_INI, "w", encoding="utf-8-sig") as f:
            self.cfg.write(f)

    def get(self, key):
        return self._get("pet", key, self.PET_DEFAULTS.get(key, 0))

    def set(self, key, val):
        self._set("pet", key, val)

    def get_daily_str(self, key):
        try:    return self.cfg.get("daily", key)
        except: return self.DAILY_DEFAULTS.get(key, "")

    def get_daily(self, key):
        return self._get("daily", key, self.DAILY_DEFAULTS.get(key, 0))

    def set_daily(self, key, val):
        self._set("daily", key, val)

    def save_all(self):
        with open(CONFIG_INI, "w", encoding="utf-8-sig") as f:
            self.cfg.write(f)

# ─────────────────────────── coin manager ────────────────────
class CoinManager:
    STREAK_COINS = {1: 20, 3: 25, 7: 35, 14: 50, 30: 80}

    def __init__(self, cfg: Config):
        self.cfg = cfg

    def balance(self) -> int:
        return max(0, self.cfg.get("coins"))

    def add(self, delta: int) -> int:
        new = max(0, self.balance() + delta)
        self.cfg.set("coins", new)
        return new

    def spend(self, amount: int) -> bool:
        if self.balance() < amount:
            return False
        self.add(-amount)
        return True

    def checkin_coins(self, streak: int) -> int:
        reward = self.STREAK_COINS.get(1, 20)
        for days, coins in self.STREAK_COINS.items():
            if streak >= days:
                reward = coins
        return reward

# ─────────────────────────── daily manager ───────────────────
class DailyManager:
    def __init__(self, cfg: Config):
        self.cfg    = cfg
        self._today = str(date.today())
        self._reset_if_new_day()

    def _reset_if_new_day(self):
        if self.cfg.get_daily_str("last_date") != self._today:
            for k in ("eat_count", "drag_count", "click_count", "sleep_count"):
                self.cfg.set_daily(k, 0)
            self.cfg.set_daily("last_date", self._today)

    def on_startup(self):
        today    = date.today()
        last_str = self.cfg.get_daily_str("last_streak_date")
        streak   = self.cfg.get_daily("streak")

        if last_str:
            try:
                last_day = date.fromisoformat(last_str)
                delta    = (today - last_day).days
            except ValueError:
                delta = 999
        else:
            delta = 999

        if delta == 1:
            streak += 1
        elif delta == 0:
            return streak, 0, None
        else:
            streak = 1

        self.cfg.set_daily("streak", streak)
        self.cfg.set_daily("last_streak_date", str(today))
        self.cfg.save_all()

        if delta > 1 and delta != 999:
            return streak, 0, "streak_break"

        bonus_aff, bonus_key = 0, None
        for days, (aff, key) in STREAK_REWARDS.items():
            if streak == days:
                bonus_aff, bonus_key = aff, key
                break

        return streak, bonus_aff, bonus_key

    def try_interact(self, action: str):
        count_key = f"{action}_count"
        limit     = DAILY_LIMITS.get(action, 0)
        used      = self.cfg.get_daily(count_key)
        if used >= limit:
            return False, 0
        self.cfg.set_daily(count_key, used + 1)
        return True, limit - used - 1

    def remaining(self, action: str) -> int:
        used  = self.cfg.get_daily(f"{action}_count")
        limit = DAILY_LIMITS.get(action, 0)
        return max(0, limit - used)

    def streak(self) -> int:
        return self.cfg.get_daily("streak")

# ─────────────────────── animation frames ────────────────────
def load_frames(prefix, count, size):
    frames = []
    for i in range(1, count + 1):
        path = pet(f"{prefix}{i}.png")
        if not os.path.exists(path): continue
        pm = QPixmap(path)
        if not pm.isNull():
            frames.append(pm.scaled(size, size,
                Qt.KeepAspectRatio, Qt.SmoothTransformation))
    return frames

# ══════════════════════════════════════════════════════════════
#  气泡控件
# ══════════════════════════════════════════════════════════════
class BubbleLabel(QWidget):
    PADDING   = (14, 8)
    MAX_WIDTH = 220
    FONT_SIZE = 13
    BG_COLOR  = QColor(255, 255, 255, 230)
    BORDER    = QColor(200, 200, 200, 180)
    TAIL_H    = 10
    BAR_H     = 8
    BAR_GAP   = 6

    def __init__(self, text, color_hex, pet_widget,
                 duration_ms=3000, progress=None, progress_label=""):
        super().__init__(None,
            Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setAttribute(Qt.WA_DeleteOnClose)
        self._pet            = pet_widget
        self._text           = text
        self._color          = QColor(color_hex)
        self._progress       = progress
        self._progress_label = progress_label

        font = QFont()
        font.setPointSize(self.FONT_SIZE)
        pw  = self.MAX_WIDTH - self.PADDING[0] * 2
        tmp = QLabel(text)
        tmp.setFont(font)
        tmp.setWordWrap(True)
        tmp.setFixedWidth(pw)
        tmp.adjustSize()
        tw = min(tmp.width(), pw)
        th = tmp.height()

        self._bw  = tw + self.PADDING[0] * 2
        extra     = (self.BAR_GAP + self.BAR_H + 4) if progress is not None else 0
        self._bh  = th + self.PADDING[1] * 2 + extra
        self.resize(self._bw, self._bh + self.TAIL_H)

        self._follow_timer = QTimer(self)
        self._follow_timer.timeout.connect(self._reposition)
        self._follow_timer.start(16)
        self._reposition()

        if duration_ms > 0:
            QTimer.singleShot(duration_ms, self.close)
        self.show()

    def _reposition(self):
        if not self._pet: return
        pg  = self._pet.geometry()
        cx  = pg.left() + pg.width()  // 2
        top = pg.top()  - self.height() - 6
        self.move(cx - self.width() // 2, top)

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)

        path = QPainterPath()
        path.addRoundedRect(QRectF(1, 1, self._bw - 2, self._bh - 2), 10, 10)
        p.fillPath(path, self.BG_COLOR)
        p.setPen(QPen(self.BORDER, 1.5))
        p.drawPath(path)

        cx = self._bw // 2
        tri = QPainterPath()
        tri.moveTo(cx - 8, self._bh)
        tri.lineTo(cx + 8, self._bh)
        tri.lineTo(cx,     self._bh + self.TAIL_H)
        tri.closeSubpath()
        p.fillPath(tri, self.BG_COLOR)
        p.setPen(QPen(self.BORDER, 1.5))
        p.drawLine(cx - 8, self._bh, cx, self._bh + self.TAIL_H)
        p.drawLine(cx + 8, self._bh, cx, self._bh + self.TAIL_H)

        p.setPen(Qt.NoPen)
        p.setBrush(self._color)
        p.drawRoundedRect(QRectF(1, 1, self._bw - 2, 4), 2, 2)

        font = QFont()
        font.setPointSize(self.FONT_SIZE)
        p.setFont(font)
        p.setPen(QColor(50, 50, 50))
        p.drawText(QRectF(self.PADDING[0], self.PADDING[1],
                          self._bw - self.PADDING[0] * 2,
                          self._bh - self.PADDING[1] * 2),
                   Qt.AlignLeft | Qt.AlignTop | Qt.TextWordWrap,
                   self._text)

        if self._progress is not None:
            bar_y = self._bh - self.PADDING[1] - self.BAR_H - 2
            bar_x = self.PADDING[0]
            bar_w = self._bw - self.PADDING[0] * 2
            p.setPen(Qt.NoPen)
            p.setBrush(QColor(220, 220, 220, 180))
            p.drawRoundedRect(QRectF(bar_x, bar_y, bar_w, self.BAR_H), 4, 4)
            fill_w = max(8.0, bar_w * max(0.0, min(1.0, self._progress)))
            grad   = QLinearGradient(bar_x, 0, bar_x + fill_w, 0)
            grad.setColorAt(0, self._color.lighter(130))
            grad.setColorAt(1, self._color)
            p.setBrush(QBrush(grad))
            p.drawRoundedRect(QRectF(bar_x, bar_y, fill_w, self.BAR_H), 4, 4)
            if self._progress_label:
                small = QFont()
                small.setPointSize(9)
                p.setFont(small)
                p.setPen(QColor(120, 120, 120))
                p.drawText(QRectF(bar_x, bar_y - 14, bar_w, 14),
                           Qt.AlignRight | Qt.AlignVCenter,
                           self._progress_label)

    def mousePressEvent(self, _):
        self.close()


# ─────────────────────── autostart helper ────────────────────
def _get_exe_path() -> str:
    """打包后返回 exe 路径，开发时返回 python 脚本路径。"""
    if getattr(sys, 'frozen', False):
        return sys.executable
    return os.path.abspath(sys.argv[0])

def is_autostart_enabled() -> bool:
    import winreg
    try:
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            0, winreg.KEY_READ)
        winreg.QueryValueEx(key, "shasha_pet")
        winreg.CloseKey(key)
        return True
    except FileNotFoundError:
        return False

def set_autostart(enabled: bool):
    import winreg
    key = winreg.OpenKey(
        winreg.HKEY_CURRENT_USER,
        r"Software\Microsoft\Windows\CurrentVersion\Run",
        0, winreg.KEY_SET_VALUE)
    if enabled:
        winreg.SetValueEx(key, "shasha_pet", 0,
                          winreg.REG_SZ, f'"{_get_exe_path()}"')
    else:
        try:
            winreg.DeleteValue(key, "shasha_pet")
        except FileNotFoundError:
            pass
    winreg.CloseKey(key)


class SettingsDialog(QDialog):
    def __init__(self, cfg, parent=None):
        super().__init__(parent,
            Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint)
        self.cfg = cfg
        # 不设 WA_TranslucentBackground，让窗口有实体背景
        layout = QFormLayout(self)
        layout.setContentsMargins(24, 24, 24, 24)
        layout.setSpacing(12)

        self.speed = QSpinBox(); self.speed.setRange(1, 20)
        self.speed.setValue(cfg.get("speed"))
        self.scale = QSpinBox(); self.scale.setRange(50, 300)
        self.scale.setValue(int(cfg.get("scale") * 100))
        self.scale.setSuffix("%")

        aff_val = cfg.get("affection")
        _, lv_name, lv_color = affection_level(aff_val)
        aff_label = QLabel(f"{aff_val}  ({lv_name})")
        aff_label.setStyleSheet(f"color:{lv_color}; font-weight:bold;")

        from PyQt5.QtWidgets import QCheckBox
        self.autostart_cb = QCheckBox()
        self.autostart_cb.setChecked(is_autostart_enabled())

        layout.addRow("移动速度:", self.speed)
        layout.addRow("缩放大小:", self.scale)
        layout.addRow("亲密度:",   aff_label)
        layout.addRow("开机自启:", self.autostart_cb)

        btns = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        btns.accepted.connect(self.accept)
        btns.rejected.connect(self.reject)
        layout.addRow(btns)

        self.setStyleSheet("""
            QDialog {
                background: #ffffff;
                border-radius: 12px;
                border: 1px solid #e0e0e0;
            }
            QLabel   { font-size:13px; color:#333; }
            QSpinBox {
                font-size:13px; padding:2px 6px;
                border:1px solid #ccc; border-radius:6px;
                background:#f9f9f9;
            }
            QCheckBox { font-size:13px; color:#333; }
            QDialogButtonBox QPushButton {
                font-size:13px; padding:4px 16px;
                border-radius:6px; border:none;
                background:#42a5f5; color:white;
            }
            QDialogButtonBox QPushButton:hover   { background:#1e88e5; }
            QDialogButtonBox QPushButton:pressed  { background:#1565c0; }
        """)

    def paintEvent(self, event):
        """手动绘制圆角白色背景。"""
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        path = QPainterPath()
        path.addRoundedRect(QRectF(self.rect()), 12, 12)
        p.fillPath(path, QColor(255, 255, 255))
        p.setPen(QPen(QColor(220, 220, 220), 1.5))
        p.drawPath(path)

    def save(self):
        self.cfg.set("speed", self.speed.value())
        self.cfg.set("scale", self.scale.value() / 100)
        set_autostart(self.autostart_cb.isChecked())

    def save(self):
        self.cfg.set("speed", self.speed.value())
        self.cfg.set("scale", self.scale.value() / 100)
        set_autostart(self.autostart_cb.isChecked())


# ══════════════════════════════════════════════════════════════
#  主宠物窗口
# ══════════════════════════════════════════════════════════════
class DesktopPet(QWidget):

    STATE_IDLE    = "idle"
    STATE_WALK    = "walk"
    STATE_EAT     = "eat"
    STATE_SLEEP   = "sleep"
    STATE_DRAG    = "drag"
    STATE_FALLING = "falling"
    STATE_HIDE    = "hide"

    def __init__(self):
        super().__init__()
        self.cfg                  = Config()
        self.daily                = DailyManager(self.cfg)
        self.coins                = CoinManager(self.cfg)
        self._fall_enabled        = True
        self._drag_pos            = QPoint()
        self._dragging            = False
        self._frame_idx           = 0
        self._walk_dir            = -1
        self._action_timer_count  = 0
        self._affection           = self.cfg.get("affection")
        self._idle_decay_counter  = 0
        self._bubble              = None
        self._last_milestone      = self._get_current_milestone()
        self._last_greeted_period = None
        self._cpu_hot             = False
        self._last_clipboard      = ""

        self._build_window()
        self._build_tray()
        self._reload_frames()
        self._state = self.STATE_IDLE
        self._set_state(self.STATE_WALK)

        QTimer.singleShot(2000, self._do_startup_streak)

        self._anim_timer = QTimer(self)
        self._anim_timer.timeout.connect(self._tick_anim)
        self._anim_timer.start(80)

        self._behav_timer = QTimer(self)
        self._behav_timer.timeout.connect(self._tick_behav)
        self._behav_timer.start(1000)

        self._bubble_timer = QTimer(self)
        self._bubble_timer.timeout.connect(self._random_bubble)
        self._reset_bubble_timer()

        self._time_timer = QTimer(self)
        self._time_timer.timeout.connect(self._check_time_greeting)
        self._time_timer.start(60_000)
        QTimer.singleShot(5000, self._check_time_greeting)

        self._cpu_timer = QTimer(self)
        self._cpu_timer.timeout.connect(self._check_cpu)
        try:
            import psutil
            self._cpu_timer.start(5_000)
        except ImportError:
            pass

        self._clipboard_timer = QTimer(self)
        self._clipboard_timer.timeout.connect(self._check_clipboard)
        self._clipboard_timer.start(1_000)

    # ── 连续登录 ──────────────────────────────────────────────
    def _do_startup_streak(self):
        streak, bonus_aff, dlg_key = self.daily.on_startup()
        coin_bonus = self.coins.checkin_coins(streak)
        self.coins.add(coin_bonus)
        self._update_tray_tooltip()

        text = ""
        if dlg_key and dlg_key in DIALOGUES:
            text = random.choice(DIALOGUES[dlg_key])
        elif streak in (2, 3, 7, 14, 30):
            key = f"streak_{streak}"
            text = random.choice(DIALOGUES.get(key, [""]))

        coin_str = f"\n签到领取 +{coin_bonus} 💰"
        aff_str  = f"\n亲密度 +{bonus_aff}" if bonus_aff > 0 else ""
        if bonus_aff > 0:
            self._add_affection(bonus_aff)

        self._show_bubble(
            (text or "签到成功！") + coin_str + aff_str,
            duration_ms=6000, show_progress=bool(bonus_aff))

    # ── 亲密度 ────────────────────────────────────────────────
    def _get_current_milestone(self):
        reached = 0
        for threshold, _, _ in AFFECTION_LEVELS:
            if self._affection >= threshold: reached = threshold
        return reached

    def _affection_progress(self):
        val        = self._affection
        cur_thresh = affection_level(val)[0]
        nxt        = next_level_threshold(val)
        if nxt is None: return 1.0, "MAX"
        return (val - cur_thresh) / (nxt - cur_thresh), f"{val}/{nxt}"

    def _add_affection(self, delta, action_key=""):
        old             = self._affection
        self._affection = max(0, self._affection + delta)
        self.cfg.set("affection", self._affection)
        self._update_aff_action()
        self._update_tray_tooltip()

        new_milestone = self._get_current_milestone()
        if new_milestone > self._last_milestone:
            self._last_milestone = new_milestone
            key = f"milestone_{new_milestone}"
            if key in DIALOGUES:
                QTimer.singleShot(400, lambda: self._show_bubble(
                    random.choice(DIALOGUES[key]),
                    duration_ms=5000, show_progress=True))
            return

        if delta < 0 and self._affection < old:
            if affection_level(self._affection)[0] < affection_level(old)[0]:
                QTimer.singleShot(200, lambda: self._show_bubble(
                    random.choice(DIALOGUES["lonely"]), duration_ms=4000))

    # ── 统一互动入口 ──────────────────────────────────────────
    def _interact(self, state):
        action_map = {
            self.STATE_EAT:   "eat",
            self.STATE_SLEEP: "sleep",
            self.STATE_DRAG:  "drag",
        }
        action = action_map.get(state)

        if action:
            allowed, remaining = self.daily.try_interact(action)
            if not allowed:
                limit_key = f"limit_{action}"
                text = random.choice(DIALOGUES.get(limit_key, ["今天已经够啦，明天再来～"]))
                self._show_bubble(text, duration_ms=4000)
                self._set_state(state)
                self._update_tray_tooltip()
                return
            delta = AFFECTION_DELTA.get(action, 0)
            self._set_state(state)
            if delta: self._add_affection(delta)
            key  = state if state in DIALOGUES else "idle"
            text = random.choice(DIALOGUES[key])
            if remaining == 0:
                text += "\n（今日次数已用完）"
            self._show_bubble(text, show_progress=True)
        else:
            self._set_state(state)
            key = state if state in DIALOGUES else "idle"
            self._show_bubble(random.choice(DIALOGUES[key]), show_progress=True)

        self._idle_decay_counter = 0
        self._update_tray_tooltip()

    def _interact_click(self):
        allowed, remaining = self.daily.try_interact("click")
        if not allowed:
            self._show_bubble(
                random.choice(DIALOGUES.get("limit_click", ["今天已经够啦！"])),
                duration_ms=4000)
            return
        delta = AFFECTION_DELTA.get("click", 2)
        self._add_affection(delta)
        self._set_state(self.STATE_EAT)
        text = random.choice(DIALOGUES.get("eat", ["好开心！"]))
        if remaining == 0:
            text += "\n（今日次数已用完）"
        self._show_bubble(text, show_progress=True)
        self._idle_decay_counter = 0
        self._update_tray_tooltip()

    # ── 气泡 ──────────────────────────────────────────────────
    def _show_bubble(self, text, duration_ms=3000, show_progress=False):
        try:
            if self._bubble and not self._bubble.isHidden():
                self._bubble.close()
        except RuntimeError:
            pass
        self._bubble = None
        _, _, color  = affection_level(self._affection)
        prog, label  = self._affection_progress() if show_progress else (None, "")
        self._bubble = BubbleLabel(text, color, self, duration_ms,
                                   progress=prog,
                                   progress_label=label if show_progress else "")

    def _random_bubble(self):
        self._reset_bubble_timer()
        key = self._state if self._state in DIALOGUES else "idle"
        self._show_bubble(random.choice(DIALOGUES[key]))

    def _reset_bubble_timer(self):
        self._bubble_timer.start(random.randint(30_000, 90_000))

    # ── CPU 监控 ──────────────────────────────────────────────
    def _check_cpu(self):
        try:
            import psutil
            usage = psutil.cpu_percent(interval=None)
        except Exception:
            return
        if usage >= 80 and not self._cpu_hot:
            self._cpu_hot = True
            self._show_bubble(random.choice(DIALOGUES["cpu_hot"]), duration_ms=6000)
        elif usage < 60 and self._cpu_hot:
            self._cpu_hot = False
            self._show_bubble(random.choice(DIALOGUES["cpu_cooling"]), duration_ms=4000)

    # ── 剪贴板互动 ────────────────────────────────────────────
    def _check_clipboard(self):
        try:
            text = QApplication.clipboard().text().strip()
        except Exception:
            return
        if not text or text == self._last_clipboard:
            return
        self._last_clipboard = text
        if re.search(r'[(){};=\[\]]', text) and len(text) > 10:
            self._show_bubble(random.choice(DIALOGUES["clipboard_code"])); return
        if len(text) > 80:
            self._show_bubble(random.choice(DIALOGUES["clipboard_long"])); return
        if re.fullmatch(r'[\u4e00-\u9fffA-Za-z]{1,20}', text):
            key = "clipboard_known" if text.lower() in _KNOWN_WORDS \
                  else "clipboard_unknown"
            self._show_bubble(random.choice(DIALOGUES[key]))

    # ── 时间感知 ──────────────────────────────────────────────
    def _get_time_period(self):
        h = datetime.now().hour
        if   h == 6:          return "morning"
        elif h == 12:         return "noon"
        elif h == 15:         return "afternoon"
        elif h == 18:         return "evening"
        elif 22 <= h <= 23:   return "night"
        elif 0  <= h <= 4:    return "midnight"
        return None

    def _check_time_greeting(self):
        period = self._get_time_period()
        if period and period != self._last_greeted_period:
            self._last_greeted_period = period
            self._show_bubble(random.choice(DIALOGUES[period]), duration_ms=5000)

    # ── window setup ──────────────────────────────────────────
    def _build_window(self):
        self.setWindowFlags(
            Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.label = QLabel(self)
        self.label.setAlignment(Qt.AlignCenter)
        self.resize(200, 200)
        screen = QApplication.primaryScreen().geometry()
        self.move(screen.width() // 2, screen.height() - 220)

    # ── tray ──────────────────────────────────────────────────
    def _build_tray(self):
        self._tray = QSystemTrayIcon(QIcon(TRAY_PNG), self)
        menu = QMenu()

        self._aff_action = QAction(menu)
        self._aff_action.setEnabled(False)
        self._update_aff_action()
        menu.addAction(self._aff_action)

        self._daily_action = QAction(menu)
        self._daily_action.setEnabled(False)
        self._update_daily_action()
        menu.addAction(self._daily_action)

        self._coin_action = QAction(menu)
        self._coin_action.setEnabled(False)
        self._update_coin_action()
        menu.addAction(self._coin_action)
        menu.addSeparator()

        def add(text, ico, slot):
            act = QAction(QIcon(icon(ico)), text, menu)
            act.triggered.connect(slot)
            menu.addAction(act)

        add("喂食", "eat.png",   lambda: self._interact(self.STATE_EAT))
        add("睡觉", "sleep.png", lambda: self._interact(self.STATE_SLEEP))
        add("隐藏", "hide.png",  self.hide)
        add("显示", "eye.png",   self._tray_show)
        menu.addSeparator()

        self._fall_action = QAction(menu)
        self._update_fall_action()
        self._fall_action.triggered.connect(self._toggle_fall)
        menu.addAction(self._fall_action)
        menu.addSeparator()

        add("锁屏", "lock.png", self._lock_screen)

        # 小游戏子菜单
        game_menu = QMenu("小游戏 🎮", menu)
        game_menu.setIcon(QIcon(icon("game.png")))

        rps_act = QAction("石头剪刀布 ✊", game_menu)
        rps_act.triggered.connect(lambda: self._launch_game(RPSGame))
        game_menu.addAction(rps_act)

        bomb_act = QAction("数字炸弹 💣", game_menu)
        bomb_act.triggered.connect(lambda: self._launch_game(BombGame))
        game_menu.addAction(bomb_act)

        slots_act = QAction("老虎机 🎰", game_menu)
        slots_act.triggered.connect(lambda: self._launch_game(SlotsGame))
        game_menu.addAction(slots_act)

        menu.addMenu(game_menu)
        menu.addSeparator()

        add("设置", "settings.png", self._open_settings)
        menu.addSeparator()
        add("重启", "restore.png",  self._restart)
        menu.addSeparator()
        add("退出", "exit.png",     QApplication.quit)

        self._tray.setContextMenu(menu)
        self._tray.show()
        self._tray.activated.connect(self._tray_activated)
        self._update_tray_tooltip()

    def _update_coin_action(self):
        self._coin_action.setText(f"💰 金币：{self.coins.balance()}")

    def _update_aff_action(self):
        _, lv_name, _ = affection_level(self._affection)
        streak = self.daily.streak()
        streak_str = f"  🔥{streak}天" if streak >= 2 else ""
        self._aff_action.setText(
            f"❤ 亲密度 {self._affection}  [{lv_name}]{streak_str}")

    def _update_daily_action(self):
        e = self.daily.remaining("eat")
        d = self.daily.remaining("drag")
        c = self.daily.remaining("click")
        s = self.daily.remaining("sleep")
        self._daily_action.setText(
            f"今日剩余  🍖×{e}  🖱×{d}  👆×{c}  💤×{s}")

    def _update_tray_tooltip(self):
        _, lv_name, _ = affection_level(self._affection)
        streak = self.daily.streak()
        streak_str = f" · 🔥{streak}天" if streak >= 2 else ""
        self._tray.setToolTip(
            f"桌面宠物 鲨鲨\n❤ {self._affection} · {lv_name}{streak_str}"
            f"\n💰 {self.coins.balance()}")
        self._update_aff_action()
        self._update_coin_action()
        self._update_daily_action()

    def _update_fall_action(self):
        if self._fall_enabled:
            self._fall_action.setText("禁用掉落")
            self._fall_action.setIcon(QIcon(icon("deviceoff.png")))
        else:
            self._fall_action.setText("开启掉落")
            self._fall_action.setIcon(QIcon(icon("deviceon.png")))

    def _toggle_fall(self):
        self._fall_enabled = not self._fall_enabled
        self._update_fall_action()
        if not self._fall_enabled and self._state == self.STATE_FALLING:
            self._set_state(self.STATE_WALK)
        elif self._fall_enabled:
            self._set_state(self.STATE_FALLING)

    # ── 小游戏 ────────────────────────────────────────────────
    def _launch_game(self, game_cls):
        game = game_cls()

        def _give_coin(delta: int):
            self.coins.add(delta)
            self._update_tray_tooltip()
            if delta > 0:
                self._show_bubble(f"+{delta} 💰", duration_ms=2000)
            elif delta < 0:
                self._show_bubble(f"{delta} 💰", duration_ms=2000)

        game.on_coin  = _give_coin
        game.on_close = self._update_tray_tooltip

        if isinstance(game, SlotsGame):
            game._get_coins   = self.coins.balance
            game._spend_coins = self.coins.spend

        game.get_widget(self).show()
        self._update_tray_tooltip()

    # ── 锁屏 ──────────────────────────────────────────────────
    def _lock_screen(self):
        import ctypes
        ctypes.windll.user32.LockWorkStation()

    # ── restart ───────────────────────────────────────────────
    def _restart(self):
        import subprocess
        subprocess.Popen([sys.executable] + sys.argv)
        QApplication.quit()

    # ── frame loading ─────────────────────────────────────────
    def _reload_frames(self):
        from PyQt5.QtGui import QTransform
        sz = int(200 * self.cfg.get("scale"))
        self.resize(sz, sz)
        self.label.resize(sz, sz)
        self._frames = {}
        for name, cnt in FRAME_COUNTS.items():
            self._frames[name] = load_frames(name, cnt, sz)
        self._frames["walkright"] = [
            f.transformed(QTransform().scale(-1, 1))
            for f in self._frames["walkleft"]
        ]
        main_pm = QPixmap(pet("main.png"))
        self._main_frame = main_pm.scaled(sz, sz,
            Qt.KeepAspectRatio, Qt.SmoothTransformation) \
            if not main_pm.isNull() else None

        # 缩放后重新钳制位置，防止窗口超出屏幕
        screen = QApplication.primaryScreen().geometry()
        x = max(0, min(self.x(), screen.width()  - sz))
        y = max(0, min(self.y(), screen.height() - sz - 40))
        self.move(x, y)

    # ── state machine ─────────────────────────────────────────
    def _set_state(self, state):
        if self._state == state: return
        self._state              = state
        self._frame_idx          = 0
        self._action_timer_count = 0

    def _tick_behav(self):
        if self._state in (self.STATE_DRAG, self.STATE_HIDE): return
        self._action_timer_count += 1
        self._idle_decay_counter += 1
        if self._idle_decay_counter >= IDLE_DECAY_INTERVAL:
            self._idle_decay_counter = 0
            self._add_affection(AFFECTION_DELTA["idle_decay"])
        if self._state == self.STATE_IDLE:
            self._set_state(self.STATE_WALK); return
        thresholds = {
            self.STATE_WALK:  self.cfg.get("interval_walk"),
            self.STATE_EAT:   self.cfg.get("interval_eat"),
            self.STATE_SLEEP: self.cfg.get("interval_sleep"),
        }
        if self._action_timer_count >= thresholds.get(self._state, 10):
            nxt = random.choices(
                [self.STATE_WALK, self.STATE_EAT, self.STATE_SLEEP],
                weights=[5, 2, 2])[0]
            self._set_state(nxt)

    # ── animation tick ────────────────────────────────────────
    def _tick_anim(self):
        if self._state == self.STATE_HIDE:
            self._do_hide_anim(); return
        frames = self._get_current_frames()
        if not frames:
            if self._main_frame: self.label.setPixmap(self._main_frame)
            return
        self._frame_idx = (self._frame_idx + 1) % len(frames)
        frame = frames[self._frame_idx]
        if self._state != self.STATE_WALK and self._walk_dir == 1:
            from PyQt5.QtGui import QTransform
            frame = frame.transformed(QTransform().scale(-1, 1))
        self.label.setPixmap(frame)
        if self._state == self.STATE_WALK:    self._do_walk()
        if self._state == self.STATE_FALLING: self._do_fall()

    def _get_current_frames(self):
        if self._state == self.STATE_WALK:
            return self._frames.get(
                "walkright" if self._walk_dir == 1 else "walkleft", [])
        mapping = {
            self.STATE_EAT: "eat", self.STATE_SLEEP: "sleep",
            self.STATE_DRAG: "drag", self.STATE_FALLING: "falling",
            self.STATE_HIDE: "hide",
        }
        return self._frames.get(mapping.get(self._state, ""), [])

    def _do_walk(self):
        speed  = self.cfg.get("speed")
        screen = QApplication.primaryScreen().geometry()
        x      = self.x() + self._walk_dir * speed
        if x <= 0:
            x = 0; self._walk_dir = 1
        elif x + self.width() >= screen.width():
            x = screen.width() - self.width(); self._walk_dir = -1
        self.move(x, self.y())

    def _do_fall(self):
        if not self._fall_enabled:
            self._set_state(self.STATE_WALK); return
        screen = QApplication.primaryScreen().geometry()
        floor  = screen.height() - self.height() - 40
        y = min(self.y() + 10, floor)
        if y >= floor: self._set_state(self.STATE_WALK)
        self.move(self.x(), y)

    def _do_hide_anim(self):
        frames = self._frames.get("hide", [])
        if not frames: self._set_state(self.STATE_WALK); return
        self._frame_idx = min(self._frame_idx + 1, len(frames) - 1)
        frame = frames[self._frame_idx]
        if self._walk_dir == 1:
            from PyQt5.QtGui import QTransform
            frame = frame.transformed(QTransform().scale(-1, 1))
        self.label.setPixmap(frame)
        if self._frame_idx == len(frames) - 1:
            self._frame_idx = 0

    # ── drag ──────────────────────────────────────────────────
    def mousePressEvent(self, e):
        if e.button() == Qt.LeftButton:
            self._dragging = True
            self._drag_pos = e.globalPos() - self.frameGeometry().topLeft()
            allowed, remaining = self.daily.try_interact("drag")
            self._set_state(self.STATE_DRAG)
            if allowed:
                self._add_affection(AFFECTION_DELTA["drag"])
                text = random.choice(DIALOGUES["drag"])
                if remaining == 0:
                    text += "\n（今日次数已用完）"
                self._show_bubble(text, show_progress=True)
            else:
                self._show_bubble(
                    random.choice(DIALOGUES.get("limit_drag", ["今天已经玩够啦～"])),
                    duration_ms=4000)
            self._idle_decay_counter = 0
            self._update_tray_tooltip()
            e.accept()

    def mouseMoveEvent(self, e):
        if self._dragging and e.buttons() & Qt.LeftButton:
            self.move(e.globalPos() - self._drag_pos); e.accept()

    def mouseReleaseEvent(self, e):
        if e.button() == Qt.LeftButton:
            self._dragging = False
            self._set_state(
                self.STATE_FALLING if self._fall_enabled else self.STATE_WALK)
            e.accept()

    def mouseDoubleClickEvent(self, e):
        if e.button() == Qt.LeftButton:
            self._interact_click()

    def contextMenuEvent(self, e):
        self._radial = RadialMenu(self)
        self._radial.show()

    def _tray_show(self):
        self.show(); self._set_state(self.STATE_WALK)

    def _tray_activated(self, reason):
        if reason == QSystemTrayIcon.DoubleClick:
            self.show(); self._set_state(self.STATE_WALK)

    def _open_settings(self):
        dlg = SettingsDialog(self.cfg, self)
        if dlg.exec_() == QDialog.Accepted:
            dlg.save(); self._reload_frames()


# ══════════════════════════════════════════════════════════════
#  径向菜单
# ══════════════════════════════════════════════════════════════
class RadialMenu(QWidget):
    ICON_SIZE     = 44
    BTN_SIZE      = 58
    RADIUS        = 150
    AUTO_CLOSE_MS = 3000

    def __init__(self, pet: "DesktopPet"):
        super().__init__(None,
            Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setAttribute(Qt.WA_DeleteOnClose)
        self._pet = pet

        self._items = [
            ("eat.png",   "喂食", lambda: pet._interact(DesktopPet.STATE_EAT)),
            ("sleep.png", "睡觉", lambda: pet._interact(DesktopPet.STATE_SLEEP)),
            ("hide.png",  "隐藏", lambda: pet._set_state(DesktopPet.STATE_HIDE)),
            ("exit.png",  "退出", QApplication.quit),
        ]

        canvas = self.RADIUS * 2 + self.BTN_SIZE
        self.resize(canvas, canvas)

        n = len(self._items)
        for i, (ico_file, tip, cb) in enumerate(self._items):
            angle = math.radians(180 + i * (180 / (n - 1)))
            cx = self.RADIUS + self.BTN_SIZE // 2 + int(self.RADIUS * math.cos(angle))
            cy = self.RADIUS + self.BTN_SIZE // 2 + int(self.RADIUS * math.sin(angle))
            btn = QPushButton(self)
            btn.setFixedSize(self.BTN_SIZE, self.BTN_SIZE)
            btn.setToolTip(tip)
            btn.move(cx - self.BTN_SIZE // 2, cy - self.BTN_SIZE // 2)
            pm = QPixmap(icon(ico_file)).scaled(
                self.ICON_SIZE, self.ICON_SIZE,
                Qt.KeepAspectRatio, Qt.SmoothTransformation)
            btn.setIcon(QIcon(pm))
            btn.setIconSize(QSize(self.ICON_SIZE, self.ICON_SIZE))
            btn.setStyleSheet("""
                QPushButton { background:transparent; border:none; border-radius:29px; }
                QPushButton:hover   { background:rgba(255,255,255,30); }
                QPushButton:pressed { background:rgba(255,255,255,100); }
            """)
            btn.clicked.connect(cb)
            btn.clicked.connect(self.close)
            btn.installEventFilter(self)

        self._reposition()

        self._follow_timer = QTimer(self)
        self._follow_timer.timeout.connect(self._reposition)
        self._follow_timer.start(16)

        self._auto_close_timer = QTimer(self)
        self._auto_close_timer.setSingleShot(True)
        self._auto_close_timer.timeout.connect(self.close)
        self._auto_close_timer.start(self.AUTO_CLOSE_MS)

    def eventFilter(self, obj, event):
        if event.type() == event.Enter:
            self._auto_close_timer.start(self.AUTO_CLOSE_MS)
        return super().eventFilter(obj, event)

    def enterEvent(self, event):
        self._auto_close_timer.start(self.AUTO_CLOSE_MS)

    def _reposition(self):
        pg = self._pet.geometry()
        self.move(pg.left() + pg.width()  // 2 - self.width()  // 2,
                  pg.top()  + pg.height() // 2 - self.height() // 4)

    def mousePressEvent(self, e):
        self.close()


# ─────────────────────────── main ────────────────────────────
if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)
    pet_widget = DesktopPet()
    pet_widget.show()
    sys.exit(app.exec_())