import unicodedata


def normalize_vi(text):
    """'Arabica Cầu Đất' -> 'arabica cau dat'"""
    if not text:
        return ""
    text = text.replace("đ", "d").replace("Đ", "D")
    text = unicodedata.normalize("NFD", text)
    text = "".join(ch for ch in text if unicodedata.category(ch) != "Mn")
    return " ".join(text.lower().split())
