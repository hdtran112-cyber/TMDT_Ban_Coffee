import logging

from django.dispatch import Signal, receiver

logger = logging.getLogger(__name__)

# Phát ra sau khi một đơn hàng được lưu thành công (kèm tham số order=...)
order_created = Signal()


@receiver(order_created)
def log_order_confirmation(sender, order, **kwargs):
    """Giả lập gửi email xác nhận: chỉ ghi một dòng log, khách không phải chờ bước này."""
    logger.info(
        "[Giả lập email] Đã gửi xác nhận đơn #%s tới %s, tổng %s đ",
        order.pk, order.user.get_username(), order.total_vnd,
    )
