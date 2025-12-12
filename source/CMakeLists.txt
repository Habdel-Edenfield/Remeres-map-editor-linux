
# *****************************************************************************
# Project remeres
# *****************************************************************************
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    project(canary-map-editor-debug LANGUAGES CXX)
else()
    project(canary-map-editor LANGUAGES CXX)
endif()

find_package(asio CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(GLUT REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
find_package(wxWidgets COMPONENTS html aui gl adv core net base REQUIRED)
include(${wxWidgets_USE_FILE})
find_package(ZLIB REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(liblzma CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

# *****************************************************************************
# Options
# *****************************************************************************
option(TOGGLE_BIN_FOLDER "Use build/bin folder for generate compilation files" OFF)
option(OPTIONS_ENABLE_OPENMP "Enable Open Multi-Processing support." ON)
option(DEBUG_LOG "Enable Debug Log" OFF)
option(SPEED_UP_BUILD_UNITY "Compile using build unity for speed up build" ON)

# LibArchive disabled in compilation level by default, see "#define OTGZ_SUPPORT" in the "definitions.h" file
#if(APPLE)
#	set(CMAKE_PREFIX_PATH /usr/local/opt/libarchive)
#endif()
# If you need use, enable this:
#find_package(LibArchive REQUIRED)
#${LibArchive_INCLUDE_DIRS} ${LibArchive_LIBRARIES}

# Build static libs
if(BUILD_STATIC_LIBRARY)
	log_option_enabled("STATIC_LIBRARY")

	if(UNIX AND NOT APPLE)
		set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	elseif(APPLE)
		set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".dylib")
	endif()
else()
	log_option_disabled("STATIC_LIBRARY")
endif()

# === DEBUG LOG ===
# cmake -DDEBUG_LOG=ON ..
if(DEBUG_LOG)
	add_definitions(-DDEBUG_LOG=ON)
	log_option_enabled("DEBUG LOG")
else()
	log_option_disabled("DEBUG LOG")
endif(DEBUG_LOG)

if (MSVC)
	add_executable(${PROJECT_NAME} "" ../cmake/remeres.rc)

	if(BUILD_STATIC_LIBRARY)
		set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()

	target_compile_options(${PROJECT_NAME} PUBLIC /MP /FS /Zf /EHsc )
else()
	add_executable(${PROJECT_NAME} "")
	if(UNIX AND NOT APPLE)
		add_definitions(-D__LINUX__)
	endif()
endif()

# === OpenMP ===
if(OPTIONS_ENABLE_OPENMP)
	log_option_enabled("openmp")
	find_package(OpenMP)
	if(OpenMP_CXX_FOUND)
		target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX)
	endif()
else()
	log_option_disabled("openmp")
endif()

# === IPO ===
check_ipo_supported(RESULT result OUTPUT output)
if(result)
	set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
	message(WARNING "IPO is not supported: ${output}")
endif()


# === UNITY BUILD (compile time reducer) ===
if(SPEED_UP_BUILD_UNITY)
	set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)
	log_option_enabled("Build unity for speed up compilation")
else()
	# === PRECOMPILED HEADER ===
	target_precompile_headers(${PROJECT_NAME} PRIVATE main.h)
endif()


target_sources(${PROJECT_NAME}
	PRIVATE
	about_window.cpp
	action.cpp
	actions_history_window.cpp
	add_item_window.cpp
	add_tileset_window.cpp
	application.cpp
	artprovider.cpp
	basemap.cpp
	brush.cpp
	brush_tables.cpp
	browse_tile_window.cpp
	border_editor_window.cpp
	positionctrl.cpp
	carpet_brush.cpp
	client_assets.cpp
	common.cpp
	common_windows.cpp
	complexitem.cpp
	container_properties_window.cpp
	copybuffer.cpp
	monster_brush.cpp
	monster.cpp
	monsters.cpp
	dat_debug_view.cpp
	dcbutton.cpp
	doodad_brush.cpp
	editor.cpp
	editor_tabs.cpp
	eraser_brush.cpp
	find_item_window.cpp
	filehandle.cpp
	graphics.cpp
	ground_brush.cpp
	gui.cpp
	house_brush.cpp
	house.cpp
	house_exit_brush.cpp
	light_drawer.cpp
	iomap.cpp
	iomap_otbm.cpp
	iominimap.cpp
	item_attributes.cpp
	item.cpp
	items.cpp
	live_action.cpp
	live_client.cpp
	live_peer.cpp
	live_server.cpp
	live_socket.cpp
	live_tab.cpp
	main_menubar.cpp
	main_toolbar.cpp
	map.cpp
	map_display.cpp
	map_drawer.cpp
	map_generator.cpp
	procedural_map_dialog.cpp
	simplex_noise.cpp
	map_region.cpp
	map_tab.cpp
	map_window.cpp
	materials.cpp
	minimap_window.cpp
	mkpch.cpp
	mt_rand.cpp
	net_connection.cpp
	npc.cpp
	npc_brush.cpp
	npcs.cpp
	numbertextctrl.cpp
	old_properties_window.cpp
	palette_brushlist.cpp
	palette_common.cpp
	palette_monster.cpp
	palette_house.cpp
	palette_npc.cpp
	palette_waypoints.cpp
	palette_zones.cpp
	palette_window.cpp
	pngfiles.cpp
	preferences.cpp
	process_com.cpp
	properties_window.cpp
	raw_brush.cpp
	replace_items_window.cpp
	result_window.cpp
	rme_net.cpp
	selection.cpp
	settings.cpp
	spawn_monster_brush.cpp
	spawn_monster.cpp
	spawn_npc.cpp
	spawn_npc_brush.cpp
	sprite_appearances.cpp
	table_brush.cpp
	templatemap76-74.cpp
	templatemap81.cpp
	templatemap854.cpp
	templatemapclassic.cpp
	tile.cpp
	tileset.cpp
	tileset_window.cpp
	town.cpp
	updater.cpp
	wall_brush.cpp
	waypoint_brush.cpp
	waypoints.cpp
	welcome_dialog.cpp
	zone_brush.cpp
	zones.cpp
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
	${CMAKE_SOURCE_DIR}/source
	${OPENGL_INCLUDE_DIR}
	${GLUT_INCLUDE_DIRS}
	${ZLIB_INCLUDE_DIR}
	${wxWidgets_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
	${OPENGL_LIBRARIES}
	${GLUT_LIBRARIES}
	${ZLIB_LIBRARIES}
	fmt::fmt
	asio::asio
	nlohmann_json::nlohmann_json
	pugixml::pugixml
	pugixml::pugixml
	${wxWidgets_LIBRARIES}
	liblzma::liblzma
	OpenGL::GL
	protobuf

	$<$<PLATFORM_ID:Linux>:xcb>
)

## Link compilation files to build/bin folder, else link to the main dir
if (TOGGLE_BIN_FOLDER)
	set_target_properties(${PROJECT_NAME}
			PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	)
	set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
else()
	set_target_properties(${PROJECT_NAME}
			PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/"
	)
	set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}")
endif()

# === Linux Icon Embedding ===
if(UNIX AND NOT APPLE)
	# Find ImageMagick convert for icon conversion
	find_program(CONVERT convert)
	
	# Use rmelinux_original.png (high quality) as primary, fallback to rmelinux.ico, then rme_icon.ico
	if(CONVERT)
		if(EXISTS "${CMAKE_SOURCE_DIR}/rmelinux_original.png")
			set(ICON_SOURCE "${CMAKE_SOURCE_DIR}/rmelinux_original.png")
			set(ICON_SOURCE_TYPE "PNG")
		elseif(EXISTS "${CMAKE_SOURCE_DIR}/rmelinux.ico")
			set(ICON_SOURCE "${CMAKE_SOURCE_DIR}/rmelinux.ico")
			set(ICON_SOURCE_TYPE "ICO")
		elseif(EXISTS "${CMAKE_SOURCE_DIR}/rme_icon.ico")
			set(ICON_SOURCE "${CMAKE_SOURCE_DIR}/rme_icon.ico")
			set(ICON_SOURCE_TYPE "ICO")
		endif()
		
		if(ICON_SOURCE)
			# Update the XPM icon used by the application code
			set(XPM_ICON "${CMAKE_SOURCE_DIR}/brushes/icon/rme_icon.xpm")
			add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
				COMMAND ${CONVERT} "${ICON_SOURCE}" -resize 48x48 -colors 256 -quality 100 "${XPM_ICON}"
				COMMENT "Atualizando ícone XPM da imagem original (alta qualidade)..."
			)
			
			# Create PNG icon files in multiple sizes for desktop integration
			set(ICON_DIR "${CMAKE_BINARY_DIR}/icons")
			set(ICON_SIZES "16;32;48;64;128;256")
			
			# Create icon directory
			file(MAKE_DIRECTORY "${ICON_DIR}")
			
			# Convert to PNG in multiple sizes with high quality and transparency preservation
			foreach(SIZE ${ICON_SIZES})
				set(ICON_PNG "${ICON_DIR}/rme_icon_${SIZE}.png")
				# Create high-quality PNG with alpha channel (transparency) preserved
				add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
					COMMAND ${CONVERT} "${ICON_SOURCE}" -resize ${SIZE}x${SIZE} -quality 100 -alpha on -define png:compression-level=9 "${ICON_PNG}"
					COMMENT "Convertendo ícone para PNG ${SIZE}x${SIZE} (alta qualidade com transparência)..."
				)
			endforeach()
			
			# Create a .desktop file for proper Linux desktop integration
			set(DESKTOP_FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.desktop")
			add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E echo "[Desktop Entry]" > "${DESKTOP_FILE}"
				COMMAND ${CMAKE_COMMAND} -E echo "Version=1.0" >> "${DESKTOP_FILE}"
				COMMAND ${CMAKE_COMMAND} -E echo "Type=Application" >> "${DESKTOP_FILE}"
				COMMAND ${CMAKE_COMMAND} -E echo "Name=Canary Map Editor" >> "${DESKTOP_FILE}"
				COMMAND ${CMAKE_COMMAND} -E echo "Comment=Map Editor for Canary Server" >> "${DESKTOP_FILE}"
				COMMAND ${CMAKE_COMMAND} -E echo "Exec=$<TARGET_FILE:${PROJECT_NAME}>" >> "${DESKTOP_FILE}"
				COMMAND ${CMAKE_COMMAND} -E echo "Icon=${ICON_DIR}/rme_icon_256.png" >> "${DESKTOP_FILE}"
				COMMAND ${CMAKE_COMMAND} -E echo "Terminal=false" >> "${DESKTOP_FILE}"
				COMMAND ${CMAKE_COMMAND} -E echo "Categories=Utility\\;Development\\;" >> "${DESKTOP_FILE}"
				COMMAND ${CMAKE_COMMAND} -E echo "StartupNotify=true" >> "${DESKTOP_FILE}"
				COMMENT "Criando arquivo .desktop para integração com desktop Linux..."
			)
			
			# Also try to embed icon using objcopy (experimental - may not work with all desktop environments)
			find_program(OBJCOPY objcopy)
			if(OBJCOPY)
				set(ICON_PNG_EMBED "${ICON_DIR}/rme_icon_256.png")
				add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
					COMMAND ${OBJCOPY} --add-section .icon="${ICON_PNG_EMBED}" --set-section-flags .icon=readonly "$<TARGET_FILE:${PROJECT_NAME}>" "$<TARGET_FILE:${PROJECT_NAME}>.tmp" 2>/dev/null || true
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>.tmp" "$<TARGET_FILE:${PROJECT_NAME}>" || ${CMAKE_COMMAND} -E remove "$<TARGET_FILE:${PROJECT_NAME}>.tmp"
					COMMENT "Tentando incorporar ícone no executável..."
					DEPENDS "${ICON_PNG_EMBED}"
				)
			endif()
		endif()
	endif()
endif()

# ============================================================================
# LINUX DESKTOP INTEGRATION - INSTALLATION
# ============================================================================
if(UNIX AND NOT APPLE)
	if(CONVERT)
		# Install desktop file to user applications directory
		# This makes the application appear in the application menu
		install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.desktop"
			DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications"
			OPTIONAL
		)

		# Install icons to hicolor icon theme (freedesktop.org standard)
		# This ensures proper icon display across different desktop environments
		foreach(SIZE ${ICON_SIZES})
			install(FILES "${ICON_DIR}/rme_icon_${SIZE}.png"
				DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/${SIZE}x${SIZE}/apps"
				RENAME "${PROJECT_NAME}.png"
				OPTIONAL
			)
		endforeach()

		# Post-install script to update icon cache
		# This refreshes the desktop environment's icon cache
		install(CODE "
			execute_process(
				COMMAND gtk-update-icon-cache -f -t ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor
				OUTPUT_QUIET
				ERROR_QUIET
			)
			execute_process(
				COMMAND update-desktop-database ${CMAKE_INSTALL_PREFIX}/share/applications
				OUTPUT_QUIET
				ERROR_QUIET
			)
			message(STATUS \"Desktop integration installed successfully\")
			message(STATUS \"Icon cache and desktop database updated\")
		")

		message(STATUS "Desktop integration install rules configured:")
		message(STATUS "  - Desktop file will be installed to: ${CMAKE_INSTALL_PREFIX}/share/applications")
		message(STATUS "  - Icons will be installed to: ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/*/apps")
		message(STATUS "  - Run 'make install' or 'cmake --install .' to install")
	endif()
endif()
