from django.contrib.auth import get_user_model
from rest_framework.test import APITestCase

from catalog.models import Product
from .models import Order, OrderStatusLog

User = get_user_model()

SHIPPING = {
    "shipping_name": "Nguyễn Văn A",
    "phone": "0900000000",
    "address": "12 Lê Lợi, Q.1, TP.HCM",
    "payment_method": "cod",
}


def make_product(name, price, stock, **extra):
    return Product.objects.create(
        name=name, price_vnd=price, stock=stock,
        bean_type="Arabica", roast_level="light", **extra
    )


class OrderApiTests(APITestCase):
    def setUp(self):
        self.user = User.objects.create_user("hieu", password="matkhau12345")
        self.other = User.objects.create_user("khac", password="matkhau12345")
        self.client.force_authenticate(self.user)
        self.p1 = make_product("Arabica", 100000, 10)
        self.p2 = make_product("Robusta", 50000, 5)

    # --- hàm hỗ trợ ---

    def add_to_cart(self, product, quantity=1):
        res = self.client.post(
            "/api/cart/items/",
            {"product_id": product.id, "quantity": quantity},
            format="json",
        )
        self.assertEqual(res.status_code, 201)

    def checkout(self, **overrides):
        return self.client.post("/api/orders/", {**SHIPPING, **overrides}, format="json")

    def place_order(self):
        """Giỏ có 2 cái p1 và 1 cái p2, đặt hàng thành công, trả về JSON của đơn."""
        self.add_to_cart(self.p1, 2)
        self.add_to_cart(self.p2, 1)
        res = self.checkout()
        self.assertEqual(res.status_code, 201)
        return res.data

    def stock(self, product):
        return Product.objects.get(pk=product.pk).stock

    # --- đặt hàng ---

    def test_checkout_requires_login(self):
        self.client.force_authenticate(None)
        self.assertEqual(self.checkout().status_code, 401)
        self.assertEqual(self.client.get("/api/orders/").status_code, 401)

    def test_checkout_creates_order(self):
        order = self.place_order()
        self.assertEqual(order["status"], "pending")
        self.assertEqual(order["total_vnd"], 2 * 100000 + 50000)
        self.assertEqual(order["shipping_name"], "Nguyễn Văn A")
        self.assertEqual(len(order["items"]), 2)
        self.assertEqual(order["payment"]["status"], "unpaid")
        self.assertEqual(order["payment"]["amount_vnd"], 250000)
        self.assertEqual([log["status"] for log in order["status_logs"]], ["pending"])
        self.assertTrue(order["can_cancel"])
        self.assertTrue(order["can_pay"])

    def test_checkout_reduces_stock(self):
        self.place_order()
        self.assertEqual(self.stock(self.p1), 8)
        self.assertEqual(self.stock(self.p2), 4)

    def test_checkout_empties_cart(self):
        self.place_order()
        cart = self.client.get("/api/cart/")
        self.assertEqual(cart.data["items"], [])
        self.assertEqual(cart.data["total_vnd"], 0)

    def test_order_keeps_price_at_purchase_time(self):
        order = self.place_order()
        Product.objects.filter(pk=self.p1.pk).update(price_vnd=999999)
        detail = self.client.get(f"/api/orders/{order['id']}/").data
        prices = {item["product_id"]: item["unit_price_vnd"] for item in detail["items"]}
        self.assertEqual(prices[self.p1.id], 100000)
        self.assertEqual(detail["total_vnd"], 250000)

    def test_checkout_ignores_price_sent_by_client(self):
        self.add_to_cart(self.p1, 1)
        res = self.checkout(total_vnd=1, unit_price_vnd=1)
        self.assertEqual(res.status_code, 201)
        self.assertEqual(res.data["total_vnd"], 100000)

    def test_checkout_empty_cart_is_rejected(self):
        res = self.checkout()
        self.assertEqual(res.status_code, 400)
        self.assertIn("cart", res.data)
        self.assertEqual(Order.objects.count(), 0)

    def test_checkout_not_enough_stock_changes_nothing(self):
        self.add_to_cart(self.p2, 3)
        Product.objects.filter(pk=self.p2.pk).update(stock=1)
        res = self.checkout()
        self.assertEqual(res.status_code, 400)
        self.assertIn("cart", res.data)
        self.assertEqual(Order.objects.count(), 0)
        self.assertEqual(self.stock(self.p2), 1)
        # Giỏ vẫn còn nguyên để khách sửa lại
        self.assertEqual(len(self.client.get("/api/cart/").data["items"]), 1)

    def test_checkout_hidden_product_is_rejected(self):
        self.add_to_cart(self.p1, 1)
        Product.objects.filter(pk=self.p1.pk).update(is_active=False)
        res = self.checkout()
        self.assertEqual(res.status_code, 400)
        self.assertEqual(Order.objects.count(), 0)

    def test_checkout_validates_shipping_info(self):
        self.add_to_cart(self.p1, 1)
        self.assertEqual(self.checkout(address="").status_code, 400)
        self.assertEqual(self.checkout(shipping_name="   ").status_code, 400)
        self.assertEqual(self.checkout(phone="123").status_code, 400)
        self.assertEqual(self.checkout(phone="abc0900000").status_code, 400)
        self.assertEqual(self.checkout(payment_method="bitcoin").status_code, 400)
        res = self.client.post("/api/orders/", {}, format="json")
        self.assertEqual(res.status_code, 400)
        self.assertEqual(Order.objects.count(), 0)

    def test_order_created_signal_writes_log(self):
        self.add_to_cart(self.p1, 1)
        with self.assertLogs("orders", level="INFO") as logs:
            with self.captureOnCommitCallbacks(execute=True):
                self.assertEqual(self.checkout().status_code, 201)
        self.assertTrue(any("hieu" in line for line in logs.output))

    # --- xem đơn ---

    def test_list_shows_only_own_orders(self):
        order = self.place_order()
        res = self.client.get("/api/orders/")
        self.assertEqual(res.status_code, 200)
        self.assertEqual([o["id"] for o in res.data], [order["id"]])
        self.assertEqual(res.data[0]["item_count"], 3)
        self.assertEqual(res.data[0]["payment_status"], "unpaid")

        self.client.force_authenticate(self.other)
        self.assertEqual(self.client.get("/api/orders/").data, [])

    def test_list_newest_first(self):
        first = self.place_order()
        self.add_to_cart(self.p1, 1)
        second = self.checkout().data
        ids = [o["id"] for o in self.client.get("/api/orders/").data]
        self.assertEqual(ids, [second["id"], first["id"]])

    def test_detail_has_items_and_history(self):
        order = self.place_order()
        res = self.client.get(f"/api/orders/{order['id']}/")
        self.assertEqual(res.status_code, 200)
        names = {item["product_name"] for item in res.data["items"]}
        self.assertEqual(names, {"Arabica", "Robusta"})
        self.assertEqual(len(res.data["status_logs"]), 1)

    def test_can_not_see_other_users_order(self):
        order = self.place_order()
        self.client.force_authenticate(self.other)
        self.assertEqual(self.client.get(f"/api/orders/{order['id']}/").status_code, 404)

    # --- hủy đơn ---

    def test_cancel_pending_order_restores_stock(self):
        order = self.place_order()
        res = self.client.post(f"/api/orders/{order['id']}/cancel/")
        self.assertEqual(res.status_code, 200)
        self.assertEqual(res.data["status"], "cancelled")
        self.assertFalse(res.data["can_cancel"])
        self.assertFalse(res.data["can_pay"])
        self.assertEqual([log["status"] for log in res.data["status_logs"]], ["pending", "cancelled"])
        self.assertEqual(self.stock(self.p1), 10)
        self.assertEqual(self.stock(self.p2), 5)

    def test_cancel_twice_does_not_restore_stock_twice(self):
        order = self.place_order()
        self.client.post(f"/api/orders/{order['id']}/cancel/")
        res = self.client.post(f"/api/orders/{order['id']}/cancel/")
        self.assertEqual(res.status_code, 400)
        self.assertEqual(self.stock(self.p1), 10)

    def test_can_not_cancel_confirmed_order(self):
        order = self.place_order()
        Order.objects.filter(pk=order["id"]).update(status="confirmed")
        res = self.client.post(f"/api/orders/{order['id']}/cancel/")
        self.assertEqual(res.status_code, 400)
        self.assertEqual(self.stock(self.p1), 8)
        self.assertEqual(OrderStatusLog.objects.filter(order_id=order["id"]).count(), 1)

    def test_can_not_cancel_paid_order(self):
        order = self.place_order()
        self.client.post(f"/api/orders/{order['id']}/pay/")
        res = self.client.post(f"/api/orders/{order['id']}/cancel/")
        self.assertEqual(res.status_code, 400)
        self.assertEqual(Order.objects.get(pk=order["id"]).status, "pending")
        self.assertEqual(self.stock(self.p1), 8)

    def test_can_not_cancel_other_users_order(self):
        order = self.place_order()
        self.client.force_authenticate(self.other)
        res = self.client.post(f"/api/orders/{order['id']}/cancel/")
        self.assertEqual(res.status_code, 404)
        self.assertEqual(Order.objects.get(pk=order["id"]).status, "pending")

    # --- thanh toán giả lập ---

    def test_pay_marks_order_paid(self):
        order = self.place_order()
        res = self.client.post(f"/api/orders/{order['id']}/pay/")
        self.assertEqual(res.status_code, 200)
        payment = res.data["payment"]
        self.assertEqual(payment["status"], "paid")
        self.assertTrue(payment["transaction_code"].startswith("TXN"))
        self.assertIsNotNone(payment["paid_at"])
        self.assertFalse(res.data["can_pay"])
        self.assertFalse(res.data["can_cancel"])

    def test_can_not_pay_twice(self):
        order = self.place_order()
        self.client.post(f"/api/orders/{order['id']}/pay/")
        res = self.client.post(f"/api/orders/{order['id']}/pay/")
        self.assertEqual(res.status_code, 400)

    def test_can_not_pay_cancelled_order(self):
        order = self.place_order()
        self.client.post(f"/api/orders/{order['id']}/cancel/")
        res = self.client.post(f"/api/orders/{order['id']}/pay/")
        self.assertEqual(res.status_code, 400)

    def test_can_not_pay_other_users_order(self):
        order = self.place_order()
        self.client.force_authenticate(self.other)
        res = self.client.post(f"/api/orders/{order['id']}/pay/")
        self.assertEqual(res.status_code, 404)
