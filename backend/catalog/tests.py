from django.contrib.auth import get_user_model
from django.test import TestCase
from rest_framework.authtoken.models import Token
from rest_framework.test import APIClient

from .models import Product


User = get_user_model()


class CatalogIntegrationTests(TestCase):
    """Kiểm thử tích hợp API catalog và xác thực token."""

    def setUp(self):
        self.client = APIClient()

        self.normal_user = User.objects.create_user(
            username="customer_test",
            password="TestPassword123!",
        )

        self.staff_user = User.objects.create_user(
            username="staff_test",
            password="TestPassword123!",
            is_staff=True,
        )

        self.product = Product.objects.create(
            name="Arabica Test",
            description="Ca phe dung de kiem thu",
            price_vnd=180000,
            stock=10,
            bean_type="Arabica",
            roast_level="light",
            weight_g=500,
            is_active=True,
        )

    def test_product_list_returns_200(self):
        response = self.client.get("/api/products/")

        self.assertEqual(response.status_code, 200)
        self.assertEqual(len(response.data), 1)
        self.assertEqual(response.data[0]["name"], "Arabica Test")

    def test_product_list_hides_inactive_products(self):
        Product.objects.create(
            name="Inactive Test",
            price_vnd=100000,
            stock=5,
            bean_type="Robusta",
            roast_level="dark",
            is_active=False,
        )

        response = self.client.get("/api/products/")

        self.assertEqual(response.status_code, 200)
        names = [item["name"] for item in response.data]

        self.assertIn("Arabica Test", names)
        self.assertNotIn("Inactive Test", names)

    def test_login_returns_token(self):
        response = self.client.post(
            "/api/auth/login/",
            {
                "username": "customer_test",
                "password": "TestPassword123!",
            },
            format="json",
        )

        self.assertEqual(response.status_code, 200)
        self.assertIn("token", response.data)

        token = Token.objects.get(user=self.normal_user)
        self.assertEqual(response.data["token"], token.key)

    def test_login_rejects_wrong_password(self):
        response = self.client.post(
            "/api/auth/login/",
            {
                "username": "customer_test",
                "password": "WrongPassword123!",
            },
            format="json",
        )

        self.assertEqual(response.status_code, 400)
        self.assertNotIn("token", response.data)

    def test_normal_user_cannot_manage_products(self):
        token = Token.objects.create(user=self.normal_user)

        self.client.credentials(
            HTTP_AUTHORIZATION=f"Token {token.key}"
        )

        response = self.client.get("/api/admin/products/")

        self.assertEqual(response.status_code, 403)

    def test_staff_user_can_manage_products(self):
        token = Token.objects.create(user=self.staff_user)

        self.client.credentials(
            HTTP_AUTHORIZATION=f"Token {token.key}"
        )

        response = self.client.get("/api/admin/products/")

        self.assertEqual(response.status_code, 200)

    def test_unauthenticated_user_cannot_manage_products(self):
        response = self.client.get("/api/admin/products/")

        self.assertIn(response.status_code, (401, 403))

    def test_delete_product_is_soft_delete(self):
        token = Token.objects.create(user=self.staff_user)

        self.client.credentials(
            HTTP_AUTHORIZATION=f"Token {token.key}"
        )

        response = self.client.delete(
            f"/api/admin/products/{self.product.pk}/"
        )

        self.assertEqual(response.status_code, 204)

        self.product.refresh_from_db()
        self.assertFalse(self.product.is_active)
