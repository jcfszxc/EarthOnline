"""
老虎机小游戏。
每次下注 10 💰，三列滚动停止后结算。
组合倍率：
  💎💎💎 × 10 → +100
  7️⃣7️⃣7️⃣  × 5  → +50
  🍒🍒🍒  × 3  → +30
  任意三同 × 2  → +20
  两同     × 0.5→ +5（小回本）
  全不同   × 0  → -10
"""
from __future__ import annotations
import random
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QLabel
)
from PyQt5.QtCore import Qt, QTimer, QRectF
from PyQt5.QtGui import QPainter, QColor, QPainterPath, QPen, QFont

from .base import PetGame

# 符号权重（越稀有权重越低）
SYMBOLS = ["💎", "7️⃣", "🍒", "🍋", "🔔", "⭐", "🍇", "🍉"]
WEIGHTS = [  2,    4,    8,   12,   12,   14,   24,   24]

BET       = 10
SPIN_MS   = 60     # 每帧滚动间隔
SPIN_STEPS = 20    # 每列滚动帧数（停止前）
COL_DELAY  = 300   # 列间停止延迟


def _calc_payout(s: list[str]) -> tuple[int, str]:
    """返回 (净收益, 描述文字)，净收益已减去下注。"""
    if s[0] == s[1] == s[2]:
        if s[0] == "💎": return 100 - BET, "💎💎💎  十倍大奖！"
        if s[0] == "7️⃣": return  50 - BET, "7️⃣7️⃣7️⃣  五倍奖励！"
        if s[0] == "🍒": return  30 - BET, "🍒🍒🍒  三倍奖励！"
        return 20 - BET, f"{s[0]*3}  三同奖励！"
    if s[0]==s[1] or s[1]==s[2] or s[0]==s[2]:
        return 5 - BET, "两同，小回本～"
    return -BET, "全不同，下次加油！"


class _RoundWindow(QWidget):
    BG   = QColor(255, 255, 255, 245)
    EDGE = QColor(200, 200, 200, 180)

    def __init__(self):
        super().__init__(None,
            Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setAttribute(Qt.WA_DeleteOnClose)

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        path = QPainterPath()
        path.addRoundedRect(QRectF(1, 1, self.width()-2, self.height()-2), 14, 14)
        p.fillPath(path, self.BG)
        p.setPen(QPen(self.EDGE, 1.5))
        p.drawPath(path)


class _SlotsWidget(_RoundWindow):
    def __init__(self, game: "SlotsGame", pet_widget: QWidget,
                 get_coins, spend_coins):
        super().__init__()
        self._game       = game
        self._pet        = pet_widget
        self._get_coins  = get_coins    # () -> int
        self._spend_coins= spend_coins  # (int) -> bool

        self._spinning   = False
        self._reels      = [random.choice(SYMBOLS) for _ in range(3)]
        self._counters   = [0, 0, 0]   # 各列已滚帧数

        self.setFixedSize(300, 380)
        self._build_ui()
        self._reposition()
        self._refresh_coin_display()

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(20, 20, 20, 20)
        root.setSpacing(12)

        title = QLabel("🎰  老虎机")
        title.setAlignment(Qt.AlignCenter)
        title.setStyleSheet("font-size:15px; font-weight:bold; color:#555;")
        root.addWidget(title)

        self._coin_label = QLabel()
        self._coin_label.setAlignment(Qt.AlignCenter)
        self._coin_label.setStyleSheet("font-size:13px; color:#f5a623;")
        root.addWidget(self._coin_label)

        # 三列老虎机
        reel_row = QHBoxLayout()
        reel_row.setSpacing(10)
        self._reel_labels = []
        for _ in range(3):
            lbl = QLabel("❓")
            lbl.setAlignment(Qt.AlignCenter)
            lbl.setFixedSize(72, 72)
            lbl.setStyleSheet(
                "font-size:36px; background:rgba(240,240,240,200);"
                "border-radius:12px;")
            reel_row.addWidget(lbl)
            self._reel_labels.append(lbl)
        root.addLayout(reel_row)

        # 结果文字
        self._result_label = QLabel("下注 10 💰，开始旋转！")
        self._result_label.setAlignment(Qt.AlignCenter)
        self._result_label.setWordWrap(True)
        self._result_label.setFixedHeight(48)
        self._result_label.setStyleSheet(
            "font-size:13px; color:#555;"
            "background:rgba(240,240,240,180); border-radius:8px; padding:4px;")
        root.addWidget(self._result_label)

        # 赔率表
        odds = QLabel("💎×10  7️⃣×5  🍒×3  三同×2  两同回本  全不同-10")
        odds.setAlignment(Qt.AlignCenter)
        odds.setWordWrap(True)
        odds.setStyleSheet("font-size:10px; color:#bbb;")
        root.addWidget(odds)

        # 按钮
        btn_row = QHBoxLayout()
        self._spin_btn = QPushButton("旋转 🎰  (-10 💰)")
        self._spin_btn.setStyleSheet(self._btn_style("#ab47bc"))
        self._spin_btn.clicked.connect(self._on_spin)
        btn_row.addWidget(self._spin_btn)

        close_btn = QPushButton("关闭")
        close_btn.setFixedWidth(70)
        close_btn.setStyleSheet(self._btn_style("#aaaaaa"))
        close_btn.clicked.connect(self.close)
        btn_row.addWidget(close_btn)
        root.addLayout(btn_row)

    @staticmethod
    def _btn_style(c):
        return (f"QPushButton{{background:{c};color:white;border:none;"
                f"border-radius:8px;font-size:13px;padding:8px;}}"
                f"QPushButton:hover{{background:{c}cc;}}"
                f"QPushButton:pressed{{background:{c}99;}}"
                f"QPushButton:disabled{{background:#ccc;}}")

    def _refresh_coin_display(self):
        self._coin_label.setText(f"💰 当前金币：{self._get_coins()}")

    # ── 旋转逻辑 ──────────────────────────────────────────────
    def _on_spin(self):
        if self._spinning: return
        if not self._spend_coins(BET):
            self._result_label.setText("💰 金币不足，明天签到领金币吧～")
            self._result_label.setStyleSheet(
                "font-size:13px;color:#ef5350;"
                "background:rgba(240,240,240,180);border-radius:8px;padding:4px;")
            return

        self._spinning = True
        self._spin_btn.setEnabled(False)
        self._result_label.setText("旋转中…")
        self._result_label.setStyleSheet(
            "font-size:13px;color:#555;"
            "background:rgba(240,240,240,180);border-radius:8px;padding:4px;")
        self._refresh_coin_display()

        # 预先决定最终结果
        self._final = random.choices(SYMBOLS, weights=WEIGHTS, k=3)
        self._counters = [0, 0, 0]

        # 每列定时器独立
        self._timers = []
        for col in range(3):
            t = QTimer(self)
            t.timeout.connect(lambda c=col: self._tick_reel(c))
            t.start(SPIN_MS + col * 10)
            self._timers.append(t)

    def _tick_reel(self, col: int):
        self._counters[col] += 1
        total_steps = SPIN_STEPS + col * (COL_DELAY // SPIN_MS)

        if self._counters[col] < total_steps:
            # 还在滚：随机显示
            self._reel_labels[col].setText(
                random.choices(SYMBOLS, weights=WEIGHTS)[0])
        else:
            # 停止：显示最终结果
            self._timers[col].stop()
            self._reel_labels[col].setText(self._final[col])
            if all(not t.isActive() for t in self._timers):
                QTimer.singleShot(200, self._settle)

    def _settle(self):
        self._spinning = False
        net, desc      = _calc_payout(self._final)
        self._game.on_coin(net)      # 正负都交给主程序处理
        self._refresh_coin_display()

        color = "#66bb6a" if net > 0 else ("#f5a623" if net == 0 else "#ef5350")
        sign  = "+" if net >= 0 else ""
        self._result_label.setText(f"{desc}  ({sign}{net} 💰)")
        self._result_label.setStyleSheet(
            f"font-size:13px;color:{color};font-weight:bold;"
            "background:rgba(240,240,240,180);border-radius:8px;padding:4px;")
        self._spin_btn.setEnabled(True)

    def _reposition(self):
        if not self._pet: return
        pg = self._pet.geometry()
        from PyQt5.QtWidgets import QApplication
        sc = QApplication.primaryScreen().geometry()
        x = max(10, min(pg.left() + pg.width()//2 - self.width()//2,
                        sc.width() - self.width() - 10))
        y = max(10, min(pg.top() - self.height() - 10,
                        sc.height() - self.height() - 10))
        self.move(x, y)

    def closeEvent(self, e):
        self._game.on_close(); super().closeEvent(e)


class SlotsGame(PetGame):
    GAME_ID   = "slots"
    GAME_NAME = "老虎机"
    # 老虎机不用 COIN_WIN/LOSE，直接由 on_coin 传净收益

    def build_widget(self, pet_widget: QWidget) -> QWidget:
        # 需要主程序注入 get_coins / spend_coins，通过 on_coin 之外额外两个回调
        return _SlotsWidget(
            self, pet_widget,
            self._get_coins,
            self._spend_coins,
        )

    # 主程序注入
    _get_coins   = lambda self: 0
    _spend_coins = lambda self, n: False