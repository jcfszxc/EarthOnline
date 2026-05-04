"""
数字炸弹小游戏。
玩家赢：+15💰  每局参与：+3💰  输：0
"""
from __future__ import annotations
import random
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QSpinBox
)
from PyQt5.QtCore import Qt, QTimer, QRectF
from PyQt5.QtGui import QPainter, QColor, QPainterPath, QPen

from .base import PetGame

RANGE_MIN, RANGE_MAX = 0, 100
PET_THINK_MS = 1200


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


class _BombWidget(_RoundWindow):
    def __init__(self, game: "BombGame", pet_widget: QWidget):
        super().__init__()
        self._game        = game
        self._pet         = pet_widget
        self._bomb        = random.randint(RANGE_MIN, RANGE_MAX)
        self._lo          = RANGE_MIN
        self._hi          = RANGE_MAX
        self._player_turn = True
        self._game_over   = False
        self.setFixedSize(300, 400)
        self._build_ui()
        self._reposition()
        self._update_ui()

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(20, 20, 20, 20)
        root.setSpacing(10)

        title = QLabel("💣  数字炸弹")
        title.setAlignment(Qt.AlignCenter)
        title.setStyleSheet("font-size:15px; font-weight:bold; color:#555;")
        root.addWidget(title)

        self._range_label = QLabel()
        self._range_label.setAlignment(Qt.AlignCenter)
        self._range_label.setStyleSheet("font-size:13px; color:#888;")
        root.addWidget(self._range_label)

        self._bomb_label = QLabel("💣")
        self._bomb_label.setAlignment(Qt.AlignCenter)
        self._bomb_label.setStyleSheet("font-size:52px;")
        self._bomb_label.setFixedHeight(70)
        root.addWidget(self._bomb_label)

        self._hint_label = QLabel()
        self._hint_label.setAlignment(Qt.AlignCenter)
        self._hint_label.setWordWrap(True)
        self._hint_label.setStyleSheet(
            "font-size:13px; color:#555;"
            "background:rgba(240,240,240,180); border-radius:8px; padding:6px;")
        self._hint_label.setFixedHeight(56)
        root.addWidget(self._hint_label)

        input_row = QHBoxLayout()
        self._spin = QSpinBox()
        self._spin.setRange(RANGE_MIN, RANGE_MAX)
        self._spin.setValue((RANGE_MIN + RANGE_MAX) // 2)
        self._spin.setStyleSheet(
            "QSpinBox{font-size:18px;padding:4px 8px;"
            "border-radius:8px;border:1px solid #ccc;}")
        self._spin.setFixedHeight(40)
        input_row.addWidget(self._spin)

        self._guess_btn = QPushButton("猜！")
        self._guess_btn.setFixedSize(70, 40)
        self._guess_btn.setStyleSheet(self._btn_style("#42a5f5"))
        self._guess_btn.clicked.connect(self._on_player_guess)
        input_row.addWidget(self._guess_btn)
        root.addLayout(input_row)

        self._pet_label = QLabel("等待你出拳…")
        self._pet_label.setAlignment(Qt.AlignCenter)
        self._pet_label.setStyleSheet("font-size:12px; color:#aaa;")
        root.addWidget(self._pet_label)

        btn_row = QHBoxLayout()
        self._replay_btn = QPushButton("再来一局")
        self._replay_btn.setStyleSheet(self._btn_style("#66bb6a"))
        self._replay_btn.setVisible(False)
        self._replay_btn.clicked.connect(self._new_game)
        btn_row.addWidget(self._replay_btn)

        close_btn = QPushButton("关闭")
        close_btn.setStyleSheet(self._btn_style("#aaaaaa"))
        close_btn.clicked.connect(self.close)
        btn_row.addWidget(close_btn)
        root.addLayout(btn_row)

    @staticmethod
    def _btn_style(c):
        return (f"QPushButton{{background:{c};color:white;border:none;"
                f"border-radius:8px;font-size:13px;}}"
                f"QPushButton:hover{{background:{c}cc;}}"
                f"QPushButton:pressed{{background:{c}99;}}"
                f"QPushButton:disabled{{background:#ccc;}}")

    def _update_ui(self):
        self._range_label.setText(f"范围：{self._lo}  ~  {self._hi}")
        self._spin.setRange(self._lo, self._hi)
        self._spin.setValue((self._lo + self._hi) // 2)
        if self._player_turn:
            self._hint_label.setText("轮到你猜啦！")
            self._hint_label.setStyleSheet(
                "font-size:13px;color:#42a5f5;"
                "background:rgba(240,240,240,180);border-radius:8px;padding:6px;")
            self._set_input_enabled(True)
            self._pet_label.setText("鲨鲨 在等你…")
        else:
            self._hint_label.setText("鲨鲨 正在思考…")
            self._hint_label.setStyleSheet(
                "font-size:13px;color:#ab47bc;"
                "background:rgba(240,240,240,180);border-radius:8px;padding:6px;")
            self._set_input_enabled(False)
            self._pet_label.setText("🤔 鲨鲨 的回合")

    def _set_input_enabled(self, en):
        self._spin.setEnabled(en)
        self._guess_btn.setEnabled(en)

    def _on_player_guess(self):
        if self._game_over or not self._player_turn: return
        self._process_guess(self._spin.value(), is_player=True)

    def _pet_guess(self):
        mid = (self._lo + self._hi) // 2
        val = max(self._lo, min(self._hi, mid + random.randint(-8, 8)))
        if val == self._lo and self._lo < self._hi:   val += 1
        elif val == self._hi and self._lo < self._hi: val -= 1
        self._pet_label.setText(f"鲨鲨 猜：{val}")
        self._process_guess(val, is_player=False)

    def _process_guess(self, val, is_player):
        who = "你" if is_player else "鲨鲨"
        if val == self._bomb:
            self._game_over = True
            self._bomb_label.setText("💥")
            if is_player:
                self._end_game(False, f"💥 {who}猜了 {val}，踩到炸弹！\n鲨鲨 赢了～")
            else:
                self._end_game(True,  f"💥 {who}猜了 {val}，踩到炸弹！\n你赢了！")
            return

        if val < self._bomb: self._lo = val + 1
        else:                self._hi = val - 1

        hint = f"{who} 猜 {val}，炸弹在 {self._lo} ~ {self._hi} 之间"
        self._hint_label.setText(hint)
        self._hint_label.setStyleSheet(
            "font-size:12px;color:#555;"
            "background:rgba(240,240,240,180);border-radius:8px;padding:6px;")

        if self._lo == self._hi:
            self._spin.setRange(self._lo, self._hi)
            self._spin.setValue(self._lo)

        self._player_turn = not is_player
        if not self._player_turn:
            self._update_ui()
            QTimer.singleShot(PET_THINK_MS, self._pet_guess)
        else:
            self._update_ui()

    def _end_game(self, player_wins, msg):
        self._set_input_enabled(False)
        color = "#66bb6a" if player_wins else "#ef5350"
        self._hint_label.setText(msg)
        self._hint_label.setStyleSheet(
            f"font-size:12px;color:{color};font-weight:bold;"
            "background:rgba(240,240,240,180);border-radius:8px;padding:6px;")
        self._replay_btn.setVisible(True)

        # 每局参与奖励
        self._game.on_coin(self._game.COIN_PLAY)
        # 胜利额外奖励
        if player_wins:
            self._game._emit_win()

    def _new_game(self):
        self._bomb        = random.randint(RANGE_MIN, RANGE_MAX)
        self._lo          = RANGE_MIN
        self._hi          = RANGE_MAX
        self._player_turn = True
        self._game_over   = False
        self._bomb_label.setText("💣")
        self._replay_btn.setVisible(False)
        self._pet_label.setText("等待你出拳…")
        self._update_ui()

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


class BombGame(PetGame):
    GAME_ID   = "bomb"
    GAME_NAME = "数字炸弹"
    COIN_WIN  = 15    # 宠物踩雷时玩家获得
    COIN_DRAW = 0
    COIN_LOSE = 0
    COIN_PLAY = 3     # 每局参与奖励

    def build_widget(self, pet_widget):
        return _BombWidget(self, pet_widget)