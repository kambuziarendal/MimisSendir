cmake_minimum_required (VERSION 3.20)

# projectname is the same as the main-executable
project(MimisSendir)

add_definitions('-gdwarf-3')
add_definitions('-Wall' '-Wextra' '-Werror' '-Wno-multichar')
add_compile_options(-fdiagnostics-color=always)
#add_compile_options(-fsanitize=address,undefined)

# Enable gloox access to Node list for XHTML-IM parsing
add_definitions('-DWANT_XHTMLIM')

set(LOCALIZED_SOURCES
	# TODO review all these files for usefulness and relevance
	${CMAKE_SOURCE_DIR}/jabber/BlabberSettings.cpp
	${CMAKE_SOURCE_DIR}/jabber/CustomStatusWindow.cpp
	${CMAKE_SOURCE_DIR}/jabber/JRoster.cpp
	${CMAKE_SOURCE_DIR}/jabber/JabberSpeak.cpp
	${CMAKE_SOURCE_DIR}/jabber/RosterView.cpp
	${CMAKE_SOURCE_DIR}/jabber/SendTalkWindow.cpp
	${CMAKE_SOURCE_DIR}/jabber/TalkManager.cpp
	${CMAKE_SOURCE_DIR}/jabber/UserID.cpp

	# "Modern" XMPP classes using gloox cleanly and directly
	${CMAKE_SOURCE_DIR}/network/GlooxHandler.cpp

	# "Modern" UI classes using gloox types and StartWatching/SendNotices
	${CMAKE_SOURCE_DIR}/ui/AboutWindow.cpp
	${CMAKE_SOURCE_DIR}/ui/AddBuddyWindow.cpp
	${CMAKE_SOURCE_DIR}/ui/BuddyInfoWindow.cpp
	${CMAKE_SOURCE_DIR}/ui/ChangeNameWindow.cpp
	${CMAKE_SOURCE_DIR}/ui/ChangeTopicWindow.cpp
	${CMAKE_SOURCE_DIR}/ui/GroupChatSettingsWindow.cpp
	${CMAKE_SOURCE_DIR}/ui/JoinGroupChat.cpp
	${CMAKE_SOURCE_DIR}/ui/MainWindow.cpp
	${CMAKE_SOURCE_DIR}/ui/RegisterAccountWindow.cpp
	${CMAKE_SOURCE_DIR}/ui/TalkView.cpp
)


add_executable(Renga
	${LOCALIZED_SOURCES}

	# TODO review all these files for usefulness and relevance
	jabber/Agent.cpp
	jabber/LoginPreferencesView.cpp
	jabber/PreferencesWindow.cpp
	jabber/RosterItem.cpp
	jabber/SoundPreferencesView.cpp
	jabber/TalkListItem.cpp
	jabber/TransportItem.cpp
	jabber/TransportPreferencesView.cpp
	jabber/BlabberApp.cpp
	jabber/AgentList.cpp
	jabber/GenericFunctions.cpp
	jabber/FileItem.cpp
	jabber/MessageRepeater.cpp
	jabber/FileXMLReader.cpp
	jabber/XMLEntity.cpp

	# Gloox extras (to be upstreamed, I guess)
	network/bob.cpp
	network/bookmark2storage.cpp
	network/confirm.cpp
	network/media.cpp
	network/references.cpp

	# "Modern" XMPP classes using gloox cleanly and directly
	network/BobStore.cpp
	network/BookmarkManager.cpp
	network/HttpUpload.cpp
	network/Giphy.cpp
	network/HttpTransfer.cpp
	network/ArchiveManager.cpp
	network/UploadManager.cpp
	network/AttachmentMetadata.cpp
	ui/AttachmentController.cpp
    ui/ActionIcon.cpp
    ui/ColorEmoji.cpp
	ui/ReactionController.cpp
    ui/AvatarCache.cpp
    ui/AvatarImage.cpp
    ui/StickerPackWindow.cpp
    network/SignalStickers.cpp
    ui/MediaPicker.cpp
    ui/MediaPreview.cpp
    support/AnimatedImage.cpp
    support/MediaLibrary.cpp
	support/ServiceKeys.cpp
	support/ProfileMigration.cpp
	support/ImageSafety.cpp
	support/ChatHistory.cpp
	support/ConversationStore.cpp
	support/CredentialStore.cpp

	# Misc. support stuff
	support/AppLocation.cpp
	support/main.cpp

	# "Modern" UI classes using gloox types and StartWatching/SendNotices
	ui/BarberPole.cpp
	ui/BookmarkItem.cpp
	ui/EditingFilter.cpp
	ui/hsluv.c
	ui/HVIFUtil.cpp
	ui/ModalAlertFactory.cpp
	ui/PeopleListItem.cpp
	ui/PeopleListView.cpp
	ui/PictureView.cpp
	ui/PopupTextControl.cpp
	ui/RosterSuperitem.cpp
	ui/RotateChatFilter.cpp
	ui/StatusView.cpp
	ui/TitledSeparatorItem.cpp
	ui/Xhtml.cpp

	${CMAKE_SOURCE_DIR}/ui/textview/Bullet.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/BulletData.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/CharacterStyle.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/CharacterStyleData.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/Paragraph.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/ParagraphLayout.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/ParagraphStyle.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/ParagraphStyleData.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/TextDocument.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/TextDocumentLayout.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/TextDocumentView.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/TextEditor.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/TextListener.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/TextSelection.cpp
	${CMAKE_SOURCE_DIR}/ui/textview/TextSpan.cpp
)

set_property(TARGET Renga PROPERTY CXX_STANDARD 20)
set_property(TARGET Renga PROPERTY OUTPUT_NAME mimissendir)

find_package(CURL 7.85 REQUIRED)
find_path(MEDIA_GIF_INCLUDE gif_lib.h REQUIRED)
find_path(MEDIA_WEBP_INCLUDE webp/demux.h REQUIRED)
find_library(MEDIA_GIF_LIBRARY NAMES gif libgif.so.7 REQUIRED)
find_library(MEDIA_WEBP_LIBRARY NAMES webp libwebp.so.7 REQUIRED)
find_library(MEDIA_WEBP_DEMUX_LIBRARY NAMES webpdemux libwebpdemux.so.2 REQUIRED)
target_include_directories(Renga PRIVATE ${MEDIA_GIF_INCLUDE} ${MEDIA_WEBP_INCLUDE})
target_link_libraries(Renga network ${MEDIA_GIF_LIBRARY} ${MEDIA_WEBP_LIBRARY} ${MEDIA_WEBP_DEMUX_LIBRARY})

find_library(gloox gloox)
if(NOT gloox)
	message(FATAL_ERROR "gloox not found, try pkgman install devel:libgloox_x86")
endif()

target_link_libraries(Renga
	# asan ubsan
	be columnlistview crypto css game gloox iconv localestub shared tracker translation wapcaplet CURL::libcurl
)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/Resource.rsrc
	COMMAND rc -o ${CMAKE_BINARY_DIR}/Resource.rsrc ${CMAKE_SOURCE_DIR}/jabber/Resource.rdef
	DEPENDS ${CMAKE_SOURCE_DIR}/jabber/Resource.rdef)
add_custom_target(RengaResources DEPENDS ${CMAKE_BINARY_DIR}/Resource.rsrc)
set_property(TARGET Renga APPEND PROPERTY LINK_DEPENDS ${CMAKE_BINARY_DIR}/Resource.rsrc)

# Embed original action icons. All rendered sizes are versioned build inputs;
# changing an icon must relink the executable before xres runs again.
file(GLOB ACTION_ICON_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/icons/actions/*.png")
set(ACTION_ICON_ARGS)
set(ACTION_ICON_ID 100)
foreach(ICON_FILE IN LISTS ACTION_ICON_FILES)
    get_filename_component(ICON_NAME "${ICON_FILE}" NAME_WE)
    list(APPEND ACTION_ICON_ARGS -a "RICO:${ACTION_ICON_ID}:${ICON_NAME}" "${ICON_FILE}")
    math(EXPR ACTION_ICON_ID "${ACTION_ICON_ID} + 1")
endforeach()
set_property(TARGET Renga APPEND PROPERTY LINK_DEPENDS ${ACTION_ICON_FILES} ${CMAKE_SOURCE_DIR}/resources/emoji/emoji.bgra)

add_custom_command(TARGET Renga POST_BUILD
	COMMAND xres -o $<TARGET_FILE:Renga> Resource.rsrc
		-a VICN:2:edit         ${CMAKE_SOURCE_DIR}/resources/icons/edit.hvif
		-a VICN:3:raise-hand   ${CMAKE_SOURCE_DIR}/resources/icons/raise-hand.hvif
		-a VICN:4:away-online  ${CMAKE_SOURCE_DIR}/resources/icons/away-online.hvif
		-a VICN:5:online       ${CMAKE_SOURCE_DIR}/resources/icons/online.hvif
		-a VICN:6:offline      ${CMAKE_SOURCE_DIR}/resources/icons/offline.hvif
		-a VICN:7:unknown      ${CMAKE_SOURCE_DIR}/resources/icons/unknown.hvif
		-a VICN:10:preferred   ${CMAKE_SOURCE_DIR}/resources/icons/preferred.hvif
		-a VICN:11:home        ${CMAKE_SOURCE_DIR}/resources/icons/home.hvif
		-a VICN:12:work        ${CMAKE_SOURCE_DIR}/resources/icons/work.hvif
        ${ACTION_ICON_ARGS}
        -a EMJI:1:emoji ${CMAKE_SOURCE_DIR}/resources/emoji/emoji.bgra
	COMMAND mimeset -f $<TARGET_FILE:Renga>
)

add_dependencies(Renga RengaResources)

add_custom_target(en.catkeys
	COMMAND ${CMAKE_C_COMPILER} -E -P -I ${CMAKE_SOURCE_DIR} -DB_COLLECTING_CATKEYS
		${LOCALIZED_SOURCES} > en.catkeys.pre
	COMMAND collectcatkeys -s "application/x-vnd.Kambuzi-MimisSendir" -w -o en.catkeys en.catkeys.pre
	DEPENDS ${LOCALIZED_SOURCES}
)

install(TARGETS Renga RUNTIME DESTINATION apps/)

install(DIRECTORY resources/Renga DESTINATION documentation/packages/mimissendir)

include_directories(${CMAKE_SOURCE_DIR})

# Testing support

#add_executable(Test
#	tests/Test.cpp
#)

#target_link_libraries(Test be)

# YouCompleteMe support

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json")
	EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
		${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
	)
endif()

# Synthetic text/inline-attachment regression checks. Requires real Haiku/app_server.
option(RENGA_BUILD_NATIVE_TESTS "Build native attachment regression tests" OFF)
if(RENGA_BUILD_NATIVE_TESTS)
    add_executable(RengaAppIconTests tests/native/AppIcon.cpp)
    target_link_libraries(RengaAppIconTests be)
    add_executable(RengaEdgeScrollTests tests/native/EdgeScroll.cpp)
    set_property(TARGET RengaEdgeScrollTests PROPERTY CXX_STANDARD 20)
    target_link_libraries(RengaEdgeScrollTests be)
    add_executable(RengaActionIconTests tests/native/ActionIcons.cpp)
    set_property(TARGET RengaActionIconTests PROPERTY CXX_STANDARD 20)
    target_link_libraries(RengaActionIconTests be translation)
endif()
if(RENGA_BUILD_NATIVE_TESTS)
	add_executable(RengaAttachmentTests
		tests/native/AttachmentText.cpp
		ui/textview/TextDocumentView.cpp
		ui/textview/TextDocumentLayout.cpp
		ui/textview/TextEditor.cpp
		ui/textview/TextSelection.cpp
		ui/textview/ParagraphLayout.cpp
		ui/textview/TextSpan.cpp
		ui/textview/CharacterStyle.cpp
		ui/textview/CharacterStyleData.cpp
		ui/textview/Paragraph.cpp
		ui/textview/ParagraphStyle.cpp
		ui/textview/ParagraphStyleData.cpp
		ui/textview/Bullet.cpp
		ui/textview/BulletData.cpp
		ui/textview/TextDocument.cpp
		ui/textview/TextListener.cpp
	)
	set_property(TARGET RengaAttachmentTests PROPERTY CXX_STANDARD 20)
	target_link_libraries(RengaAttachmentTests be)
endif()

if(RENGA_BUILD_NATIVE_TESTS)
    add_executable(RengaConversationTests tests/conversations.cpp support/ConversationStore.cpp)
    set_property(TARGET RengaConversationTests PROPERTY CXX_STANDARD 20)
endif()

if(RENGA_BUILD_NATIVE_TESTS)
    add_executable(RengaDeliveryTests tests/delivery.cpp)
    set_property(TARGET RengaDeliveryTests PROPERTY CXX_STANDARD 20)
    add_executable(RengaMetadataTests tests/attachments/metadata.cpp
        network/HttpUpload.cpp network/AttachmentMetadata.cpp)
    set_property(TARGET RengaMetadataTests PROPERTY CXX_STANDARD 20)
    target_link_libraries(RengaMetadataTests gloox)
endif()

if(RENGA_BUILD_NATIVE_TESTS)
    add_executable(RengaUiProbe tests/native/UiProbe.cpp)
    set_property(TARGET RengaUiProbe PROPERTY CXX_STANDARD 20)
    target_link_libraries(RengaUiProbe be)
endif()

if(RENGA_BUILD_NATIVE_TESTS)
    add_executable(RengaMediaTests tests/media.cpp support/AnimatedImage.cpp support/MediaLibrary.cpp support/ImageSafety.cpp)
    set_property(TARGET RengaMediaTests PROPERTY CXX_STANDARD 20)
    target_include_directories(RengaMediaTests PRIVATE ${MEDIA_GIF_INCLUDE} ${MEDIA_WEBP_INCLUDE})
    target_link_libraries(RengaMediaTests crypto ${MEDIA_GIF_LIBRARY} ${MEDIA_WEBP_LIBRARY} ${MEDIA_WEBP_DEMUX_LIBRARY})
endif()

if(RENGA_BUILD_NATIVE_TESTS)
    add_executable(RengaReactionTests tests/reactions.cpp network/HttpUpload.cpp network/AttachmentMetadata.cpp)
    set_property(TARGET RengaReactionTests PROPERTY CXX_STANDARD 20)
    target_link_libraries(RengaReactionTests gloox)
endif()

if(RENGA_BUILD_NATIVE_TESTS)
    add_executable(RengaHistoryTests tests/native/History.cpp support/ChatHistory.cpp ui/ColorEmoji.cpp)
    set_property(TARGET RengaHistoryTests PROPERTY CXX_STANDARD 20)
    target_link_libraries(RengaHistoryTests be crypto)
    set_property(TARGET RengaHistoryTests APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/resources/emoji/emoji.bgra)
    add_custom_command(TARGET RengaHistoryTests POST_BUILD COMMAND xres -o $<TARGET_FILE:RengaHistoryTests> -a EMJI:1:emoji ${CMAKE_SOURCE_DIR}/resources/emoji/emoji.bgra)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    add_executable(RengaGiphyTests tests/native/Giphy.cpp network/Giphy.cpp support/ServiceKeys.cpp)
    target_link_libraries(RengaGiphyTests PRIVATE be shared CURL::libcurl)
    target_compile_features(RengaGiphyTests PRIVATE cxx_std_17)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    add_executable(RengaPackImportTests tests/pack_import.cpp support/MediaLibrary.cpp support/AnimatedImage.cpp support/ImageSafety.cpp)
    target_include_directories(RengaPackImportTests PRIVATE ${MEDIA_GIF_INCLUDE} ${MEDIA_WEBP_INCLUDE})
    target_link_libraries(RengaPackImportTests PRIVATE ${MEDIA_GIF_LIBRARY} ${MEDIA_WEBP_LIBRARY} ${MEDIA_WEBP_DEMUX_LIBRARY} crypto)
    target_compile_features(RengaPackImportTests PRIVATE cxx_std_20)
    add_executable(RengaPreviewTests tests/native/MediaPreview.cpp ui/MediaPreview.cpp ui/AvatarImage.cpp support/AnimatedImage.cpp support/ImageSafety.cpp)
    target_include_directories(RengaPreviewTests PRIVATE ${MEDIA_GIF_INCLUDE} ${MEDIA_WEBP_INCLUDE})
    target_link_libraries(RengaPreviewTests PRIVATE be translation ${MEDIA_GIF_LIBRARY} ${MEDIA_WEBP_LIBRARY} ${MEDIA_WEBP_DEMUX_LIBRARY})
    target_compile_features(RengaPreviewTests PRIVATE cxx_std_17)
endif()

if(RENGA_BUILD_NATIVE_TESTS)
    add_executable(RengaStickerTests tests/signal_stickers.cpp network/SignalStickers.cpp network/HttpTransfer.cpp network/HttpUpload.cpp)
    target_compile_features(RengaStickerTests PRIVATE cxx_std_20)
    target_link_libraries(RengaStickerTests PRIVATE crypto gloox CURL::libcurl)
    add_executable(RengaDateTests tests/chat_date.cpp)
    target_compile_features(RengaDateTests PRIVATE cxx_std_17)
endif()

add_executable(RengaStickerLink support/StickerLink.cpp)
target_link_libraries(RengaStickerLink PRIVATE be)
set_property(TARGET RengaStickerLink APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/support/StickerLink.rdef)
add_custom_command(TARGET RengaStickerLink POST_BUILD
    COMMAND rc -o ${CMAKE_BINARY_DIR}/StickerLink.rsrc ${CMAKE_SOURCE_DIR}/support/StickerLink.rdef
    COMMAND xres -o ${CMAKE_BINARY_DIR}/RengaStickerLink ${CMAKE_BINARY_DIR}/StickerLink.rsrc
    COMMAND mimeset -f $<TARGET_FILE:RengaStickerLink>)

if(RENGA_BUILD_NATIVE_TESTS)
    add_executable(RengaSettingsTests tests/native/Settings.cpp jabber/FileXMLReader.cpp)
    target_compile_features(RengaSettingsTests PRIVATE cxx_std_20)
    target_link_libraries(RengaSettingsTests PRIVATE be gloox network)
endif()

if(RENGA_BUILD_NATIVE_TESTS)
    add_executable(MimisSendirProfileTests tests/profile_migration.cpp support/ProfileMigration.cpp)
    target_compile_features(MimisSendirProfileTests PRIVATE cxx_std_20)
endif()

if(RENGA_BUILD_NATIVE_TESTS)
    add_executable(MimisSendirServiceKeyTests tests/native/ServiceKeys.cpp support/ServiceKeys.cpp)
    target_compile_features(MimisSendirServiceKeyTests PRIVATE cxx_std_20)
    target_link_libraries(MimisSendirServiceKeyTests PRIVATE be)
endif()
