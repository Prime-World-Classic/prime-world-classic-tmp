#pragma once

// Аппаратные фичи, влияющие на выбор путей рендера.
//
// Из набора удалены vendor-специфичные расширения D3D9-эпохи:
//   hasDF16/hasDF24/hasRAWZ/hasINTZ/hasRESZ — форматы глубины ATI/NV,
//     читаемые как текстура. Под Vulkan/DX12 ту же роль играет depth SRV
//     (D24S8/D32F), отдельного флага не требуется.
//   hasR2VB / hasVT16 / hasVT32 — Render-to-VB и vertex textures.
//     Заменяются structured/typed buffer'ами.
//   hasNLRT — null render target. Заменяется color-write маской.
// hasD16T/hasD24X/hasD24S оставлены как единый гейт «аппаратные карты
// теней читаются как depth-текстура» (см. HasDSM()).
//
// Область видимости — namespace Render (раньше enum объявлялся внутри тела
// IConfigManager через #include в struct; теперь его заполняет ещё и бэкенд,
// а дублировать enum в двух областях нельзя).
namespace Render
{
enum Features {
  hasD16T = 1,            // Depth/Stencil textures (hardware shadow maps) - D3DFMT_D16
  hasD24X = hasD16T << 1, // Depth/Stencil textures (hardware shadow maps) - D3DFMT_D24X8
  hasD24S = hasD24X << 1, // Depth/Stencil textures (hardware shadow maps) - D3DFMT_D24S8
  // «Глубина читается как текстура» в смысле Vulkan/DX12 (depth SRV), а не в
  // смысле D3D9. Разница принципиальная: D3D9 читает глубину из текстуры
  // только через vendor-форматы (INTZ/RAWZ/DF16/DF24), которые в этом стволе
  // удалены (a639d8319), а hasD16T/hasD24X/hasD24S — это ответ
  // CheckDeviceFormat(DEPTHSTENCIL, TEXTURE), который под Wine/DXVK говорит
  // «есть» даже там, где читаемой глубины нет (на реальном D3D9-железе — нет,
  // см. docs/NVRHI_VULKAN_VS_DXVK_ISSUES.md, проблемы 10 и 11). Ставит бит
  // только NVRHI-бэкенд; D3D9-бэкенд его не выставляет ни при каких caps.
  hasDepthSRV = hasD24S << 1,
};

// Меняется при несовместимом изменении набора флагов (в отчёты уходит
// только маска caps, см. renderer.cpp). 3 = добавлен hasDepthSRV.
static const int featuresVersion = 3;
}; // namespace Render
