# games 包入口，统一暴露所有游戏类
from .rps   import RPSGame
from .bomb  import BombGame
from .slots import SlotsGame

__all__ = ["RPSGame", "BombGame", "SlotsGame"]