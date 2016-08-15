set(MAN_NAMES nsrlsvr.1 nsrlupdate.1)
add_custom_target(man ALL DEPENDS ${MAN_NAMES})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/nsrlsvr.1 DESTINATION
${CMAKE_INSTALL_PREFIX}/man/man1/)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/nsrlupdate.1 DESTINATION
${CMAKE_INSTALL_PREFIX}/man/man1/)
