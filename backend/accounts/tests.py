from django.contrib.auth import get_user_model
from django.test import TestCase
from rest_framework.authtoken.models import Token
from rest_framework.test import APIClient

User = get_user_model()


class AccountsApiTests(TestCase):
    """Kiểm thử API tài khoản: đăng ký, đăng nhập, xem thông tin bản thân."""

    def setUp(self):
        self.client = APIClient()

    def test_register_creates_customer(self):
        response = self.client.post(
            "/api/auth/register/",
            {"username": "khach1", "email": "khach1@example.com", "password": "MatKhauManh123!"},
            format="json",
        )
        self.assertEqual(response.status_code, 201)
        user = User.objects.get(username="khach1")
        self.assertFalse(user.is_staff)
        self.assertTrue(user.check_password("MatKhauManh123!"))
        self.assertNotIn("password", response.data)

    def test_register_rejects_duplicate_username(self):
        User.objects.create_user(username="khach1", password="MatKhauManh123!")
        response = self.client.post(
            "/api/auth/register/",
            {"username": "KHACH1", "email": "a@example.com", "password": "MatKhauManh123!"},
            format="json",
        )
        self.assertEqual(response.status_code, 400)
        self.assertIn("username", response.data)

    def test_register_rejects_weak_password(self):
        response = self.client.post(
            "/api/auth/register/",
            {"username": "khach2", "email": "b@example.com", "password": "12345678"},
            format="json",
        )
        self.assertEqual(response.status_code, 400)
        self.assertIn("password", response.data)

    def test_register_cannot_make_staff(self):
        response = self.client.post(
            "/api/auth/register/",
            {"username": "hacker", "email": "c@example.com",
             "password": "MatKhauManh123!", "is_staff": True},
            format="json",
        )
        self.assertEqual(response.status_code, 201)
        self.assertFalse(User.objects.get(username="hacker").is_staff)

    def test_me_requires_token(self):
        response = self.client.get("/api/auth/me/")
        self.assertIn(response.status_code, (401, 403))

    def test_me_returns_current_user(self):
        user = User.objects.create_user(username="khach3", password="MatKhauManh123!")
        token = Token.objects.create(user=user)
        self.client.credentials(HTTP_AUTHORIZATION=f"Token {token.key}")
        response = self.client.get("/api/auth/me/")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.data["username"], "khach3")
        self.assertFalse(response.data["is_staff"])
