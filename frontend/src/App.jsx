import { Route, Routes } from "react-router-dom";
import Layout from "./components/Layout";
import ProductDetailPage from "./modules/catalog/ProductDetailPage";
import ProductListPage from "./modules/catalog/ProductListPage";
import CartPage from "./modules/orders/CartPage";
import CheckoutPage from "./modules/orders/CheckoutPage";
import OrderDetailPage from "./modules/orders/OrderDetailPage";
import OrdersPage from "./modules/orders/OrdersPage";

function ComingSoon({ title }) {
  return <p>{title}: đang làm.</p>;
}

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
        <Route path="/trace" element={<ComingSoon title="Truy xuất mẻ rang (Cường)" />} />
        <Route path="/login" element={<ComingSoon title="Đăng nhập (Anh)" />} />
        <Route path="*" element={<p>Không tìm thấy trang.</p>} />
      </Route>
    </Routes>
  );
}
