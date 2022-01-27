# add opening file format to info.plist
function(add_file_format_info_plist)
	cmake_parse_arguments(ARG "" "TARGET;FILE;FORMAT" "" ${ARGN})
	
	string(TOUPPER ${ARG_FORMAT} FORMAT_UPPER)
	string(TOLOWER ${ARG_FORMAT} FORMAT_LOWER)
	
	add_custom_command(
		TARGET ${ARG_TARGET}
		POST_BUILD
		COMMAND plutil -insert CFBundleDocumentTypes.0 -xml '<dict><key>CFBundleTypeName</key><string>${FORMAT_UPPER} 3D File</string><key>CFBundleTypeIconFile</key><string>meshlab.icns</string><key>CFBundleTypeRole</key><string>Editor</string><key>LSHandlerRank</key><string>Default</string></dict>' ${ARG_FILE}
		COMMAND plutil -insert CFBundleDocumentTypes.0.CFBundleTypeExtensions -xml '<array/>' ${ARG_FILE}
		COMMAND plutil -insert CFBundleDocumentTypes.0.CFBundleTypeExtensions.0 -xml '<string>${FORMAT_LOWER}</string>' ${ARG_FILE}
		COMMAND plutil -insert CFBundleDocumentTypes.0.CFBundleTypeOSTypes -xml '<array/>' ${ARG_FILE}
		COMMAND plutil -insert CFBundleDocumentTypes.0.CFBundleTypeOSTypes.0 -xml '<string>${FORMAT_UPPER}</string>' ${ARG_FILE}
	)
endfunction()

# add additional settings to info.plist
function(set_additional_settings_info_plist)
	cmake_parse_arguments(ARG "" "TARGET;FILE" "" ${ARGN})
	add_custom_command(
		TARGET ${ARG_TARGET}
		POST_BUILD
		COMMAND plutil -replace NSHighResolutionCapable -bool True ${ARG_FILE}
		COMMAND plutil -replace CFBundleDocumentTypes -xml '<array/>' ${ARG_FILE}
		COMMAND plutil -replace CFBundleIdentifier -string 'com.vcg.nxsview' ${ARG_FILE}
	)

	add_file_format_info_plist(
		TARGET ${ARG_TARGET}
		FILE ${ARG_FILE}
		FORMAT NXS)
	add_file_format_info_plist(
		TARGET ${ARG_TARGET}
		FILE ${ARG_FILE}
		FORMAT NXZ)
endfunction()
