#ifndef ISOLATE_NETNS_H
#define ISOLATE_NETNS_H

#include <stdio.h>
#include <string.h>
#include <linux/rtnetlink.h>
#include <linux/veth.h>
#include <net/if.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

/**
 * @def MAX_PAYLOAD
 * @brief Максимальный размер полезной нагрузки для Netlink сообщений.
 */
#define MAX_PAYLOAD 1024

/**
 * @struct nl_req
 * @brief Структура для формирования Netlink запроса с информацией об интерфейсе.
 */
struct nl_req {
    struct nlmsghdr n;      /**< Заголовок Netlink сообщения */
    struct ifinfomsg i;     /**< Структура с информацией об интерфейсе */
    char buf[MAX_PAYLOAD];  /**< Буфер полезной нагрузки */
};

/**
 * @def NLMSG_TAIL(nmsg)
 * @brief Получить указатель на хвост (структуру rtattr) Netlink сообщения.
 * 
 * @param nmsg Указатель на Netlink сообщение
 * @return Указатель на хвостовое rtattr сообщение
 */
#define NLMSG_TAIL(nmsg) \
    ((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

/**
 * @brief Создает сокет для работы с сетевыми операциями.
 * 
 * @param domain Домен сокета (например, AF_NETLINK)
 * @param type Тип сокета (например, SOCK_RAW)
 * @param protocol Протокол сокета (например, NETLINK_ROUTE)
 * @return Дескриптор созданного сокета или -1 в случае ошибки.
 */
int create_socket(int domain, int type, int protocol);

/**
 * @brief Активирует сетевой интерфейс и назначает ему IP адрес и маску.
 * 
 * @param ifname Имя интерфейса (например, "veth0")
 * @param ip IP адрес в строковом формате (например, "192.168.1.10")
 * @param netmask Маска подсети в строковом формате (например, "255.255.255.0")
 */
void if_up(char *ifname, char *ip, char *netmask);

/**
 * @brief Создает пару виртуальных Ethernet интерфейсов (veth pair).
 * 
 * @param sock_fd Дескриптор сокета Netlink
 * @param ifname Имя первого интерфейса пары
 * @param peername Имя второго интерфейса пары
 */
void create_veth(int sock_fd, char *ifname, char *peername);

/**
 * @brief Перемещает сетевой интерфейс в сетевое пространство имен процесса.
 * 
 * @param sock_fd Дескриптор сокета Netlink
 * @param ifname Имя интерфейса, который нужно переместить
 * @param netns Дескриптор сетевого пространства имен (file descriptor)
 */
void move_if_to_pid_netns(int sock_fd, char *ifname, int netns);

/**
 * @brief Получает файловый дескриптор сетевого пространства имен заданного PID.
 * 
 * @param pid Идентификатор процесса (PID)
 * @return Дескриптор сетевого пространства имен или -1 при ошибке
 */
int get_netns_fd(int pid);

#endif //ISOLATE_NETNS_H
