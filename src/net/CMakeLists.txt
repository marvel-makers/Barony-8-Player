list(APPEND GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/packet.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/connections.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/metrics.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/network.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/messages.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/reliability.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lifecycle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/transport/transport.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/transport/udp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/transport/compat.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/transport/steam_sockets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/transport/eos.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/protocol/entity.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/protocol/player.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/protocol/lobby.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/protocol/client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/protocol/server.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/protocol/session.cpp"
)

set(GAME_SOURCES ${GAME_SOURCES} PARENT_SCOPE)
