"""
所有桌宠小游戏的抽象基类。
新游戏只需继承 PetGame，实现 build_widget() 即可。
亲密度由日常互动负责，游戏只产出金币。
"""
from __future__ import annotations
from typing import Callable, Optional
from PyQt5.QtWidgets import QWidget


class PetGame:
    GAME_ID    : str = ""
    GAME_NAME  : str = ""
    DAILY_LIMIT: int = 99     # 默认不限局数

    # 子类定义金币奖励
    COIN_WIN  : int = 0
    COIN_DRAW : int = 0
    COIN_LOSE : int = 0

    def __init__(self):
        self._widget: Optional[QWidget] = None
        # 主程序注入：给予 / 扣除金币
        self.on_coin : Callable[[int], None] = lambda delta: None
        self.on_close: Callable[[], None]    = lambda: None

    def get_widget(self, pet_widget: QWidget) -> QWidget:
        if self._widget is None or not self._widget.isVisible():
            self._widget = self.build_widget(pet_widget)
        return self._widget

    def build_widget(self, pet_widget: QWidget) -> QWidget:
        raise NotImplementedError

    # 子类调用
    def _emit_win(self):
        self.on_coin(self.COIN_WIN)

    def _emit_draw(self):
        self.on_coin(self.COIN_DRAW)

    def _emit_lose(self):
        self.on_coin(self.COIN_LOSE)