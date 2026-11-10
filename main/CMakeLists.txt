# =============================================================================
# main 组件：板级外设驱动 + GUI Guider 生成的 UI
#
# GUI Guider 导出的目录结构：
#   main/ui/generated : 屏幕(screens)、事件(events)、字体/图片(assets)、
#                       gui_guider.h、gg_utils.c/h
#   main/ui/custom    : custom.c/h、lv_conf_ext.h 等用户自定义代码
#
# 这里把两个 UI 目录下的所有 .c 文件都编译进 main 组件，并把生成代码
# 引用到的头文件目录全部加入 include 路径。重新用 GUI Guider 生成代码后，
# 只要文件还在上述目录中，本文件无需再改。
# =============================================================================

set(MAIN_DIR          "${CMAKE_CURRENT_SOURCE_DIR}")
set(UI_GENERATED_DIR  "${MAIN_DIR}/ui/generated")
set(UI_CUSTOM_DIR     "${MAIN_DIR}/ui/custom")

# 板级/外设驱动源文件（保持显式列出，避免和 UI 代码混在一起）
set(MAIN_SOURCES
    "main.c"
    "lv_prot.c"
    "xl9555.c"
    "ft6336u_driver.c"
)

# GUI Guider 生成的所有源文件：screens / events / assets(fonts,images) / custom
# 注意：ESP-IDF 会在脚本模式下读取本文件来解析组件依赖，
# 该模式不支持 CONFIGURE_DEPENDS，因此这里使用普通 GLOB。
# 若 UI 中新增了 .c 文件，重新执行一次 cmake/idf.py reconfigure 即可。
file(GLOB_RECURSE UI_SOURCES
    "${UI_GENERATED_DIR}/*.c"
    "${UI_CUSTOM_DIR}/*.c"
)

# GUI Guider 生成代码用到的头文件搜索路径：
#   gui_guider.h  -> assets/fonts/gg_font.h、assets/images/gg_image.h
#   gg_home.c     -> gg_utils.h、events/gg_event.h
#   gg_event_*.c  -> custom.h
set(UI_INCLUDE_DIRS
    "${UI_GENERATED_DIR}"
    "${UI_GENERATED_DIR}/assets"
    "${UI_GENERATED_DIR}/assets/fonts"
    "${UI_GENERATED_DIR}/assets/images"
    "${UI_GENERATED_DIR}/events"
    "${UI_GENERATED_DIR}/screens"
    "${UI_CUSTOM_DIR}"
)

idf_component_register(
    SRCS ${MAIN_SOURCES} ${UI_SOURCES}
    INCLUDE_DIRS "${MAIN_DIR}" ${UI_INCLUDE_DIRS}
)
