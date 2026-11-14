from django.apps import AppConfig


class OrdersConfig(AppConfig):
    name = 'orders'

    def ready(self):
        # Nạp file signals để hàm nhận sự kiện order_created được đăng ký
        from . import signals  # noqa: F401
