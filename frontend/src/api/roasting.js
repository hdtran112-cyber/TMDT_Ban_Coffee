import { http } from "./http";

export async function getTrace(code) {
  const cleanCode = encodeURIComponent(code.trim());
  // Chỉ để /trace/, hàm http() sẽ tự ghép thành /api/trace/...
  return await http(`/trace/${cleanCode}/`);
}