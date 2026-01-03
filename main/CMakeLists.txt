set(srcs
    "main.c"
)
set(incs
    "."
)

# 使用wifi sta组件
list(APPEND srcs "components/wifi/my_sta.c")
list(APPEND incs "components/wifi/include")

# 使用ucp组件
list(APPEND srcs "components/net/ucp/src/ucp.c"
                 "components/net/ucp/src/ucp_recv.c"
                 "components/net/ucp/src/ucp_send.c"
                 "components/net/ucp/src/utils/os_interfaces.c"
                 "components/net/ucp/src/utils/os_socket.c"
                 "components/net/ucp/src/utils/os_udp.c"
                 "components/net/ucp/src/utils/utils.c")
list(APPEND incs "components/net/ucp/include")

# 使用app/my_ucp
list(APPEND srcs "app/my_ucp.c")
list(APPEND incs "app")

idf_component_register(SRCS ${srcs}
                    PRIV_REQUIRES esp_wifi nvs_flash esp_timer
                    INCLUDE_DIRS ${incs})
