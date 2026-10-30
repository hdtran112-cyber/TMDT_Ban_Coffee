import { Route, Routes } from "react-router-dom";
import Layout from "./components/Layout";
import ProductDetailPage from "./modules/catalog/ProductDetailPage";
import ProductListPage from "./modules/catalog/ProductListPage";
import CartPage from "./modules/orders/CartPage";

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
        <Route path="/orders" element={<ComingSoon title="Đơn của tôi (Hiếu)" />} />
        <Route path="/trace" element={<ComingSoon title="Truy xuất mẻ rang (Cường)" />} />
        <Route path="/login" element={<ComingSoon title="Đăng nhập (Anh)" />} />
        <Route path="*" element={<p>Không tìm thấy trang.</p>} />
      </Route>
    </Routes>
  );
}
