"""
石头剪刀布小游戏。
胜：+8金币  平：+2金币  负：0
"""
from __future__ import annotations
import random
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QLabel
)
from PyQt5.QtCore import Qt, QRectF
from PyQt5.QtGui import QPainter, QColor, QPainterPath, QPen

from .base import PetGame

CHOICES = ["石头", "剪刀", "布"]
EMOJI   = {"石头": "✊", "剪刀": "✌️", "布": "🖐️"}
WINS    = {"石头": "剪刀", "剪刀": "布", "布": "石头"}

def judge(player, pet):
    if player == pet:       return "draw"
    if WINS[player] == pet: return "win"
    return "lose"

RESULT_TEXT = {
    "win":  ("你赢了！",  "#66bb6a", "+8 💰"),
    "draw": ("平局～",    "#42a5f5", "+2 💰"),
    "lose": ("你输了…",  "#ef5350", "+0 💰"),
}


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


class _RPSWidget(_RoundWindow):
    def __init__(self, game: "RPSGame", pet_widget: QWidget):
        super().__init__()
        self._game  = game
        self._pet   = pet_widget
        self._score = [0, 0]
        self.setFixedSize(300, 360)
        self._build_ui()
        self._reposition()

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(18, 18, 18, 18)
        root.setSpacing(10)

        title = QLabel("✊✌️🖐️  石头剪刀布")
        title.setAlignment(Qt.AlignCenter)
        title.setStyleSheet("font-size:15px; font-weight:bold; color:#555;")
        root.addWidget(title)

        self._score_label = QLabel("你 0 : 0 鲨鲨")
        self._score_label.setAlignment(Qt.AlignCenter)
        self._score_label.setStyleSheet("font-size:13px; color:#888;")
        root.addWidget(self._score_label)

        self._result_area = QLabel("请出拳！")
        self._result_area.setAlignment(Qt.AlignCenter)
        self._result_area.setFixedHeight(90)
        self._result_area.setStyleSheet(
            "font-size:32px; background:rgba(240,240,240,180); border-radius:10px;")
        root.addWidget(self._result_area)

        self._result_text = QLabel("")
        self._result_text.setAlignment(Qt.AlignCenter)
        self._result_text.setStyleSheet("font-size:14px; font-weight:bold;")
        root.addWidget(self._result_text)

        self._coin_label = QLabel("")
        self._coin_label.setAlignment(Qt.AlignCenter)
        self._coin_label.setStyleSheet("font-size:12px; color:#aaa;")
        root.addWidget(self._coin_label)

        btn_row = QHBoxLayout()
        btn_row.setSpacing(8)
        self._btns = {}
        for c in CHOICES:
            btn = QPushButton(f"{EMOJI[c]}\n{c}")
            btn.setFixedSize(74, 64)
            btn.setStyleSheet(self._btn_style("#42a5f5"))
            btn.clicked.connect(lambda _, ch=c: self._on_choose(ch))
            btn_row.addWidget(btn)
            self._btns[c] = btn
        root.addLayout(btn_row)

        self._replay_btn = QPushButton("再来一局")
        self._replay_btn.setVisible(False)
        self._replay_btn.setStyleSheet(self._btn_style("#66bb6a"))
        self._replay_btn.clicked.connect(self._reset_round)

        close_btn = QPushButton("关闭")
        close_btn.setStyleSheet(self._btn_style("#aaaaaa"))
        close_btn.clicked.connect(self.close)

        action_row = QHBoxLayout()
        action_row.addWidget(self._replay_btn)
        action_row.addWidget(close_btn)
        root.addLayout(action_row)

    @staticmethod
    def _btn_style(c):
        return (f"QPushButton{{background:{c};color:white;border:none;"
                f"border-radius:10px;font-size:13px;}}"
                f"QPushButton:hover{{background:{c}cc;}}"
                f"QPushButton:pressed{{background:{c}99;}}"
                f"QPushButton:disabled{{background:#ccc;}}")

    def _on_choose(self, player_choice):
        self._set_btns_enabled(False)
        pet_choice = random.choice(CHOICES)
        result     = judge(player_choice, pet_choice)

        if result == "win":   self._score[0] += 1
        elif result == "lose": self._score[1] += 1
        self._score_label.setText(f"你 {self._score[0]} : {self._score[1]} 鲨鲨")

        self._result_area.setText(f"{EMOJI[player_choice]}  vs  {EMOJI[pet_choice]}")
        txt, color, coin_str = RESULT_TEXT[result]
        self._result_text.setText(txt)
        self._result_text.setStyleSheet(
            f"font-size:14px; font-weight:bold; color:{color};")
        self._coin_label.setText(coin_str)

        if   result == "win":  self._game._emit_win()
        elif result == "draw": self._game._emit_draw()
        else:                  self._game._emit_lose()

        self._replay_btn.setVisible(True)

    def _reset_round(self):
        self._result_area.setText("请出拳！")
        self._result_text.setText("")
        self._coin_label.setText("")
        self._replay_btn.setVisible(False)
        self._set_btns_enabled(True)

    def _set_btns_enabled(self, en):
        for b in self._btns.values(): b.setEnabled(en)

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


class RPSGame(PetGame):
    GAME_ID  = "rps"
    GAME_NAME = "石头剪刀布"
    COIN_WIN  = 8
    COIN_DRAW = 2
    COIN_LOSE = 0

    def build_widget(self, pet_widget):
        return _RPSWidget(self, pet_widget)