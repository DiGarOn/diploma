from __future__ import annotations

PI0 = [6, 8, 2, 3, 9, 10, 5, 12, 1, 14, 4, 7, 11, 13, 0, 15]
PI1 = [12, 4, 6, 2, 10, 5, 11, 9, 14, 8, 13, 7, 0, 3, 15, 1]


class MiniGost:
    """
    МиниГОСТ - упрощенная версия блочного шифра ГОСТ 28147-89.

    Формат:
    - блок: 16 бит
    - ключ: 32 бита
    - K = (K^(4), K^(3), K^(2), K^(1)), где K^(4) - старший байт, K^(1) - младший байт
    - порядок раундовых ключей (12 раундов):
      K^(1), K^(2), K^(3), K^(4), K^(1), K^(2), K^(3), K^(4), K^(4), K^(3), K^(2), K^(1)
    """

    def __init__(self, pi0: list[int] | None = None, pi1: list[int] | None = None) -> None:
        self.pi0 = PI0[:] if pi0 is None else list(pi0)
        self.pi1 = PI1[:] if pi1 is None else list(pi1)
        self._validate_sbox(self.pi0, "pi0")
        self._validate_sbox(self.pi1, "pi1")

    @staticmethod
    def _validate_sbox(sbox: list[int], name: str) -> None:
        if len(sbox) != 16:
            raise ValueError(f"{name} must contain exactly 16 elements")
        if sorted(sbox) != list(range(16)):
            raise ValueError(f"{name} must be a permutation of numbers 0..15")

    @staticmethod
    def _rotl8(x: int, n: int) -> int:
        x &= 0xFF
        n &= 7
        return ((x << n) | (x >> (8 - n))) & 0xFF

    def _Pi(self, x: int) -> int:
        x &= 0xFF
        x1 = (x >> 4) & 0x0F  # старшая тетрада
        x0 = x & 0x0F         # младшая тетрада
        return ((self.pi1[x1] & 0x0F) << 4) | (self.pi0[x0] & 0x0F)

    def g(self, y: int, x: int) -> int:
        return self._rotl8((self._Pi((x + y) & 0xFF)), 5)

    def _round(self, y: int, left: int, right: int) -> tuple[int, int]:
        return right, left ^ self.g(y, right)

    def _inverse_round(self, y: int, left: int, right: int) -> tuple[int, int]:
        return right ^ self.g(y, left), left

    @staticmethod
    def _split_block(block: int) -> tuple[int, int]:
        if not 0 <= block <= 0xFFFF:
            raise ValueError("block must be a 16-bit integer (0..0xFFFF)")
        return (block >> 8) & 0xFF, block & 0xFF

    @staticmethod
    def _join_block(left: int, right: int) -> int:
        return ((left & 0xFF) << 8) | (right & 0xFF)

    @staticmethod
    def _split_key(key: int) -> tuple[int, int, int, int]:
        if not 0 <= key <= 0xFFFFFFFF:
            raise ValueError("key must be a 32-bit integer (0..0xFFFFFFFF)")
        k4 = (key >> 24) & 0xFF
        k3 = (key >> 16) & 0xFF
        k2 = (key >> 8) & 0xFF
        k1 = key & 0xFF
        return k4, k3, k2, k1

    def _round_keys(self, key: int) -> list[int]:
        k4, k3, k2, k1 = self._split_key(key)
        return [k1, k2, k3, k4, k1, k2, k3, k4, k4, k3, k2, k1]

    def encrypt_block(self, block: int, key: int) -> int:
        left, right = self._split_block(block)
        for rk in self._round_keys(key):
            left, right = self._round(rk, left, right)
        left, right = right, left  # T
        return self._join_block(left, right)

    def decrypt_block(self, block: int, key: int) -> int:
        left, right = self._split_block(block)
        left, right = right, left  # T^{-1} = T
        for rk in reversed(self._round_keys(key)):
            left, right = self._inverse_round(rk, left, right)
        return self._join_block(left, right)

    def encrypt_block_bytes(self, block: bytes, key: bytes) -> bytes:
        if len(block) != 2:
            raise ValueError("block must contain exactly 2 bytes")
        if len(key) != 4:
            raise ValueError("key must contain exactly 4 bytes")
        block_int = int.from_bytes(block, byteorder="big")
        key_int = int.from_bytes(key, byteorder="big")
        return self.encrypt_block(block_int, key_int).to_bytes(2, byteorder="big")

    def decrypt_block_bytes(self, block: bytes, key: bytes) -> bytes:
        if len(block) != 2:
            raise ValueError("block must contain exactly 2 bytes")
        if len(key) != 4:
            raise ValueError("key must contain exactly 4 bytes")
        block_int = int.from_bytes(block, byteorder="big")
        key_int = int.from_bytes(key, byteorder="big")
        return self.decrypt_block(block_int, key_int).to_bytes(2, byteorder="big")


if __name__ == "__main__":
    cipher = MiniGost()

    key = 0xA1B2C3D4
    block = 0x1234

    encrypted = cipher.encrypt_block(block, key)
    decrypted = cipher.decrypt_block(encrypted, key)

    print(f"key       = 0x{key:08X}")
    print(f"plain     = 0x{block:04X}")
    print(f"encrypted = 0x{encrypted:04X}")
    print(f"decrypted = 0x{decrypted:04X}")
