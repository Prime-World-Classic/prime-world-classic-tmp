#pragma once
// Маркер RenderDoc-капчи (env PW_VK_RDCAP*) — общий для обоих бэкендов.
//
// Капчу снимает НЕ клиент. Под Wine гостевой PE не может вызвать
// RENDERDOC_GetAPI (символ живёт в хостовом librenderdoc.so), а LD_PRELOAD-
// хуки инжекта не видит winevulkan. Работает другой контур: слой
// VK_LAYER_RENDERDOC_Capture включает ХОСТОВЫЙ Vulkan-загрузчик внутри wine64
// (VK_ADD_IMPLICIT_LAYER_PATH=<...>/implicit_layer.d +
// ENABLE_VULKAN_RENDERDOC_CAPTURE=1), в процессе поднимается target-control
// сервер слоя (порты 38920..38927), куда хост шлёт TriggerCapture/QueueCapture
// — tools/rd/rdctl.py (протокол: renderdoc/core/target_control.cpp). Слой
// считает кадры по present'ам, и счётчик present'ов бэкенда с ним совпадает
// (замер 2026-10-22: «Finished capture, Frame 1899» при presents=1899), так
// что хост армит захват ровно на нужном кадре. Роль клиента — часы сессии
// (секунды от первого present'а) и файл-маркер с целевым кадром.
//
// Маркер нужен ОБОИМ бэкендам. Без него DXVK-контур снимается вслепую по
// wall-clock, и «пара» капч оказывается из разных моментов мира: у капч
// pw_frame2314 и dx_frame9105 число инстансов у инстанс-рисунков эффектов
// разошлось (289 против 339), поэтому «пропущенный» или «лишний» рисунок на
// такой паре доказать нельзя (docs/NVRHI_VULKAN_VS_DXVK_ISSUES.md, проблемы
// 5—7 и 12). С маркером у обоих бэкендов пара берётся на одной СЕКУНДЕ
// сессии — а не на одном номере кадра: частота у контуров разная (замер
// 2026-10-20: DXVK ~5,5 к/с против ~40 у Vulkan на этом стенде).
//
// Строка маркера одна на оба бэкенда — её читает один и тот же rdwatch.py:
//   PWRDCAP 1 seq=<n> cur=<present> frame=<cur+lead> sec=<сек> api=<0|1>
//
// Env (одинаковые для обоих бэкендов):
//   PW_VK_RDCAP_TIME=<сек>   секунда от первого present'а
//   PW_VK_RDCAP=<кадр>       то же, но по номеру present'а
//   PW_VK_RDCAP_STEP=<сек>   0 — одна капча, N — серия через N секунд
//   PW_VK_RDCAP_LEAD=<кадр>  запас на внешнюю реакцию (дефолт 60)
//   PW_VK_RDCAP_FILE=<имя>   маркер (дефолт rdcap.marker; cwd = Game/Bin,
//                            имя БЕЗ '/': под Wine путь с '/' в аргументе
//                            портится)
//   PW_VK_RDC_DLL=1          грузить renderdoc.dll самим (нативный Windows-
//                            контур); под Wine её нет, маркер пишется в любом
//                            случае
//
// Включать ПОСЛЕ stdafx.h — нужен макрос DebugTrace (System/SystemLog.h).

#include "Vendor/renderdoc/renderdoc_app.h"

// Единственная точка входа app API: с библиотекой не линкуемся, берём
// GetProcAddress из уже загруженной dll (свою грузим только по PW_VK_RDC_DLL=1).
inline RENDERDOC_API_1_7_0* RdCapApi(bool* outHasTitle = 0)
{
  static RENDERDOC_API_1_7_0* api = 0;
  static bool tried = false;
  static bool title = false;   // dll отдала struct 1.6.0+ (SetCaptureTitle)

  if (outHasTitle)
    *outHasTitle = title;
  if (tried)
    return api;
  tried = true;

  HMODULE h = GetModuleHandleA("renderdoc.dll");
  if (!h)
  {
    char rf[8];
    if (GetEnvironmentVariableA("PW_VK_RDC_DLL", rf, sizeof(rf) - 1) > 0 && atoi(rf))
      h = LoadLibraryA("renderdoc.dll");
  }
  if (!h)
    return 0;
  pRENDERDOC_GetAPI getApi = (pRENDERDOC_GetAPI)GetProcAddress(h, "RENDERDOC_GetAPI");
  if (!getApi)
    return 0;
  // Просим 1.7.0; если dll старше — просим 1.0.0 и дальше трогаем только те
  // поля, которые в старой dll точно есть (в 1.46 все RENDERDOC_API_1_x_y —
  // typedef одного struct, см. Vendor/renderdoc/renderdoc_app.h:835, но у
  // старой dll struct короче).
  void* out = 0;
  if (getApi(eRENDERDOC_API_Version_1_7_0, &out) && out)
    title = true;
  else
  {
    out = 0;
    if (!getApi(eRENDERDOC_API_Version_1_0_0, &out) || !out)
      return 0;
  }
  api = (RENDERDOC_API_1_7_0*)out;
  int major = 0, minor = 0, patch = 0;
  if (api->GetAPIVersion)
    api->GetAPIVersion(&major, &minor, &patch);
  DebugTrace("RdCap: RenderDoc API %d.%d.%d в процессе", major, minor, patch);
  if (outHasTitle)
    *outHasTitle = title;
  return api;
}

// Часы сессии + маркер. Бэкенд вызывает Init() при создании устройства,
// Due()/Mark() — из Present().
struct RdCapMarker
{
  unsigned at = 0;           // PW_VK_RDCAP: кадр (present), на котором капча
  unsigned timeS = 0;        // PW_VK_RDCAP_TIME: секунда сессии
  unsigned step = 0;         // PW_VK_RDCAP_STEP: период серии в секундах
  unsigned lead = 60;        // PW_VK_RDCAP_LEAD: кадров запаса на арм
  unsigned lastSec = 0xffffffffu;  // уже отмеченная секунда (серия)
  unsigned seq = 0;          // счётчик маркеров (нумерует серии)
  unsigned long long t0 = 0; // GetTickCount64() на первом present'е
  char marker[64] = "rdcap.marker";

  void Init()
  {
    char rc[16];
    if (GetEnvironmentVariableA("PW_VK_RDCAP", rc, sizeof(rc) - 1) > 0)
      at = (unsigned)atoi(rc);
    if (GetEnvironmentVariableA("PW_VK_RDCAP_TIME", rc, sizeof(rc) - 1) > 0)
      timeS = (unsigned)atoi(rc);
    if (GetEnvironmentVariableA("PW_VK_RDCAP_STEP", rc, sizeof(rc) - 1) > 0)
      step = (unsigned)atoi(rc);
    if (GetEnvironmentVariableA("PW_VK_RDCAP_LEAD", rc, sizeof(rc) - 1) > 0)
      lead = (unsigned)atoi(rc);
    if (GetEnvironmentVariableA("PW_VK_RDCAP_FILE", marker, sizeof(marker) - 1) <= 0)
      strcpy(marker, "rdcap.marker");
  }

  bool Enabled() const { return at || timeS; }

  // Вызывается из Present'а. presents — счётчик present'ов бэкенда (он же, что
  // счётчик кадров слоя). Заполняет sec/target, возвращает true, если сейчас
  // обязан быть маркер.
  bool Due(unsigned presents, unsigned& secOut, unsigned& targetOut)
  {
    if (!Enabled())
      return false;
    if (!t0)
      t0 = GetTickCount64();
    const unsigned el = (unsigned)((GetTickCount64() - t0) / 1000ull);
    bool one = false;
    if (timeS)
    {
      // PW_VK_RDCAP_STEP=0 — одна капча на секунде timeS; N>0 — серия
      // timeS, timeS+N, timeS+2N, ...
      if (step)
      {
        one = el >= timeS && ((el - timeS) % step) == 0u && el != lastSec;
        if (one)
          lastSec = el;
      }
      else
        one = seq == 0u && el >= timeS;
    }
    else
      one = seq == 0u && presents >= at;
    if (!one)
      return false;
    secOut = el;
    targetOut = presents + lead;
    return true;
  }

  // sec — секунда сессии, target — кадр, на котором хост обязан снять капчу.
  void Mark(unsigned presents, unsigned sec, unsigned target, const char* tag)
  {
    ++seq;
    bool title = false;
    RENDERDOC_API_1_7_0* api = RdCapApi(&title);
    // Имя маркера относительное и БЕЗ '/': под Wine путь с '/' в аргументе
    // портится (тот же вывод, что у PW_VK_RECORD), а cwd = Game/Bin, так что
    // на хосте файл лежит в <стенд>/Game/Bin/<маркер>.
    FILE* f = fopen(marker, "wb");
    if (f)
    {
      fprintf(f, "PWRDCAP 1 seq=%u cur=%u frame=%u sec=%u api=%d\n",
              seq, presents, target, sec, api ? 1 : 0);
      fclose(f);
    }
    else
      DebugTrace("%s: маркер %s открыть не удалось", tag, marker);
    if (api)
    {
      if (title && api->SetCaptureTitle)
      {
        char t[48];
        snprintf(t, sizeof(t), "match t=%us frame=%u", sec, target);
        api->SetCaptureTitle(t);
      }
      api->TriggerCapture();
    }
    DebugTrace("%s: маркер %s seq=%u sec=%u present=%u frame=%u api=%d",
               tag, marker, seq, sec, presents, target, api ? 1 : 0);
  }
};
