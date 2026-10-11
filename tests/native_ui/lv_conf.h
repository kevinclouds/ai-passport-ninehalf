#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (64 * 1024)
#define LV_USE_OS LV_OS_NONE
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_NULL 1
#define LV_USE_THORVG_INTERNAL 0
#ifndef __ASSEMBLER__
#include <stdlib.h>
#endif
#define LV_ASSERT_HANDLER abort();
#define LV_LOG_PRINTF 1

#define LV_FONT_FMT_TXT_LARGE 1
