import { Route, Routes } from "react-router-dom";
import Layout from "./components/Layout";
import ProductListPage from "./modules/catalog/ProductListPage";

function ComingSoon({ title }) {
  return <p>{title}: đang làm.</p>;
}

export default function App() {
  return (
    <Routes>
      <Route element={<Layout />}>
        <Route path="/" element={<ProductListPage />} />
        <Route path="/cart" element={<ComingSoon title="Giỏ hàng (Hiếu)" />} />
        <Route path="/orders" element={<ComingSoon title="Đơn của tôi (Hiếu)" />} />
        <Route path="/trace" element={<ComingSoon title="Truy xuất mẻ rang (Cường)" />} />
        <Route path="/login" element={<ComingSoon title="Đăng nhập (Anh)" />} />
        <Route path="*" element={<p>Không tìm thấy trang.</p>} />
      </Route>
    </Routes>
  );
}
