from django.contrib.auth import get_user_model
from rest_framework.test import APITestCase

from catalog.models import Product
from .models import CartItem

User = get_user_model()


def make_product(name, price, stock, **extra):
    return Product.objects.create(
        name=name, price_vnd=price, stock=stock,
        bean_type="Arabica", roast_level="light", **extra
    )


class CartApiTests(APITestCase):
    def setUp(self):
        self.user = User.objects.create_user("hieu", password="matkhau12345")
        self.other = User.objects.create_user("khac", password="matkhau12345")
        self.client.force_authenticate(self.user)
        self.p1 = make_product("Arabica", 100000, 10)
        self.p2 = make_product("Robusta", 50000, 5)

    def add(self, product, quantity=1):
        return self.client.post(
            "/api/cart/items/",
            {"product_id": product.id, "quantity": quantity},
            format="json",
        )

    def test_can_not_use_cart_without_login(self):
        self.client.force_authenticate(None)
        self.assertEqual(self.client.get("/api/cart/").status_code, 401)
        self.assertEqual(self.add(self.p1).status_code, 401)

    def test_empty_cart(self):
        res = self.client.get("/api/cart/")
        self.assertEqual(res.status_code, 200)
        self.assertEqual(res.data["items"], [])
        self.assertEqual(res.data["total_vnd"], 0)

    def test_add_new_product(self):
        res = self.add(self.p1, 2)
        self.assertEqual(res.status_code, 201)
        self.assertEqual(len(res.data["items"]), 1)
        self.assertEqual(res.data["items"][0]["quantity"], 2)
        self.assertEqual(res.data["items"][0]["line_total_vnd"], 200000)
        self.assertEqual(res.data["total_vnd"], 200000)

    def test_add_same_product_accumulates(self):
        self.add(self.p1, 2)
        res = self.add(self.p1, 3)
        self.assertEqual(res.status_code, 201)
        self.assertEqual(len(res.data["items"]), 1)
        self.assertEqual(res.data["items"][0]["quantity"], 5)
        self.assertEqual(res.data["total_vnd"], 500000)

    def test_total_of_many_products(self):
        self.add(self.p1, 1)
        res = self.add(self.p2, 2)
        self.assertEqual(res.data["total_vnd"], 100000 + 2 * 50000)

    def test_add_over_stock_is_rejected(self):
        res = self.add(self.p2, 6)
        self.assertEqual(res.status_code, 400)
        self.assertIn("quantity", res.data)
        self.assertEqual(CartItem.objects.count(), 0)

    def test_accumulating_over_stock_is_rejected(self):
        self.add(self.p2, 3)
        res = self.add(self.p2, 3)
        self.assertEqual(res.status_code, 400)
        self.assertEqual(CartItem.objects.get().quantity, 3)

    def test_add_hidden_product_is_rejected(self):
        hidden = make_product("Da an", 90000, 5, is_active=False)
        self.assertEqual(self.add(hidden).status_code, 400)

    def test_add_unknown_product_is_rejected(self):
        res = self.client.post(
            "/api/cart/items/", {"product_id": 999999, "quantity": 1}, format="json"
        )
        self.assertEqual(res.status_code, 400)

    def test_add_out_of_stock_product_is_rejected(self):
        empty = make_product("Het hang", 90000, 0)
        self.assertEqual(self.add(empty).status_code, 400)

    def test_quantity_must_be_at_least_one(self):
        self.assertEqual(self.add(self.p1, 0).status_code, 400)
        self.assertEqual(self.add(self.p1, -2).status_code, 400)
        res = self.client.post(
            "/api/cart/items/", {"product_id": self.p1.id, "quantity": "abc"}, format="json"
        )
        self.assertEqual(res.status_code, 400)

    def test_change_quantity(self):
        item_id = self.add(self.p1, 1).data["items"][0]["id"]
        res = self.client.patch(
            f"/api/cart/items/{item_id}/", {"quantity": 4}, format="json"
        )
        self.assertEqual(res.status_code, 200)
        self.assertEqual(res.data["items"][0]["quantity"], 4)
        self.assertEqual(res.data["total_vnd"], 400000)

    def test_change_quantity_over_stock_is_rejected(self):
        item_id = self.add(self.p2, 1).data["items"][0]["id"]
        res = self.client.patch(
            f"/api/cart/items/{item_id}/", {"quantity": 50}, format="json"
        )
        self.assertEqual(res.status_code, 400)
        self.assertEqual(CartItem.objects.get(pk=item_id).quantity, 1)

    def test_change_quantity_to_zero_is_rejected(self):
        item_id = self.add(self.p1, 1).data["items"][0]["id"]
        res = self.client.patch(
            f"/api/cart/items/{item_id}/", {"quantity": 0}, format="json"
        )
        self.assertEqual(res.status_code, 400)

    def test_remove_item(self):
        item_id = self.add(self.p1, 1).data["items"][0]["id"]
        res = self.client.delete(f"/api/cart/items/{item_id}/")
        self.assertEqual(res.status_code, 204)
        cart = self.client.get("/api/cart/")
        self.assertEqual(cart.data["items"], [])
        self.assertEqual(cart.data["total_vnd"], 0)

    def test_carts_are_separate_per_user(self):
        self.add(self.p1, 1)
        self.client.force_authenticate(self.other)
        res = self.client.get("/api/cart/")
        self.assertEqual(res.data["items"], [])

    def test_can_not_touch_other_users_item(self):
        item_id = self.add(self.p1, 1).data["items"][0]["id"]
        self.client.force_authenticate(self.other)
        patch = self.client.patch(
            f"/api/cart/items/{item_id}/", {"quantity": 3}, format="json"
        )
        delete = self.client.delete(f"/api/cart/items/{item_id}/")
        self.assertEqual(patch.status_code, 404)
        self.assertEqual(delete.status_code, 404)
        self.assertEqual(CartItem.objects.get(pk=item_id).quantity, 1)

    def test_item_flagged_unavailable_when_stock_drops(self):
        self.add(self.p1, 5)
        Product.objects.filter(pk=self.p1.pk).update(stock=2)
        item = self.client.get("/api/cart/").data["items"][0]
        self.assertFalse(item["available"])

    def test_item_flagged_unavailable_when_product_hidden(self):
        self.add(self.p1, 1)
        Product.objects.filter(pk=self.p1.pk).update(is_active=False)
        item = self.client.get("/api/cart/").data["items"][0]
        self.assertFalse(item["available"])
