import { Route, Routes } from "react-router-dom";
import Layout from "./components/Layout";
import ProductDetailPage from "./modules/catalog/ProductDetailPage";
import ProductListPage from "./modules/catalog/ProductListPage";
import CartPage from "./modules/orders/CartPage";
import CheckoutPage from "./modules/orders/CheckoutPage";
import OrderDetailPage from "./modules/orders/OrderDetailPage";
import OrdersPage from "./modules/orders/OrdersPage";
import TracePage from "./modules/roasting/TracePage";
import AuthPage from "./modules/accounts/AuthPage";

export default function App() {
  return (
    <Routes>
      <Route element={<Layout />}>
        <Route path="/" element={<ProductListPage />} />
        <Route path="/products/:id" element={<ProductDetailPage />} />
        <Route path="/cart" element={<CartPage />} />
        <Route path="/checkout" element={<CheckoutPage />} />
        <Route path="/orders" element={<OrdersPage />} />
        <Route path="/orders/:id" element={<OrderDetailPage />} />
        <Route path="/trace" element={<TracePage />} />
        <Route path="/trace/:code" element={<TracePage />} />
        <Route path="/login" element={<AuthPage mode="login" />} />
        <Route path="/register" element={<AuthPage mode="register" />} />
        <Route path="*" element={<p>Không tìm thấy trang.</p>} />
      </Route>
    </Routes>
  );
}