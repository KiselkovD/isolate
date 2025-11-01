#include <stdio.h>
#include <string.h>
#include <linux/rtnetlink.h>
#include <linux/veth.h>
#include <net/if.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "../include/netns.h"
#include "../include/util.h"

/**
 * @brief Добавляет атрибут (RT attribute) к Netlink сообщению.
 * 
 * @param n Указатель на Netlink заголовок
 * @param maxlen Максимально допустимый размер сообщения
 * @param type Тип атрибута (rta_type)
 * @param data Указатель на данные для атрибута (может быть NULL)
 * @param datalen Размер данных в байтах
 */
static void addattr_l(
        struct nlmsghdr *n, int maxlen, __u16 type,
        const void *data, __u16 datalen)
{
    __u16 attr_len = RTA_LENGTH(datalen);

    __u32 newlen = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(attr_len);
    if (newlen > maxlen)
        die("cannot add attribute. size (%d) exceeded maxlen (%d)\n",
            newlen, maxlen);

    struct rtattr *rta = NLMSG_TAIL(n);
    rta->rta_type = type;
    rta->rta_len = attr_len;
    if (datalen)
        memcpy(RTA_DATA(rta), data, datalen);

    n->nlmsg_len = newlen;
}

/**
 * @brief Начинает вложенный атрибут (nested rtattr) в Netlink сообщении.
 * 
 * @param n Указатель на Netlink сообщение
 * @param maxlen Максимально допустимый размер сообщения
 * @param type Тип вложенного атрибута
 * @return Указатель на начало вложенного атрибута для последующего закрытия
 */
static struct rtattr *addattr_nest(
        struct nlmsghdr *n, int maxlen, __u16 type)
{
    struct rtattr *nest = NLMSG_TAIL(n);
    addattr_l(n, maxlen, type, NULL, 0);
    return nest;
}

/**
 * @brief Завершает вложенный атрибут, устанавливая правильную длину.
 * 
 * @param n Указатель на Netlink сообщение
 * @param nest Указатель на вложенный атрибут, начинавшийся addattr_nest()
 */
static void addattr_nest_end(struct nlmsghdr *n, struct rtattr *nest)
{
    nest->rta_len = (void *)NLMSG_TAIL(n) - (void *)nest;
}

/**
 * @brief Считывает ответ из Netlink сокета.
 * 
 * @param fd Дескриптор Netlink сокета
 * @param msg Структура msghdr для recvmsg
 * @param response Указатель на буфер для сохранения ответа
 * @return Размер принятого сообщения, или завершение программы при ошибке
 */
static ssize_t read_response(
        int fd, struct msghdr *msg, char **response)
{
    struct iovec *iov = msg->msg_iov;
    iov->iov_base = *response;
    iov->iov_len = MAX_PAYLOAD;

    ssize_t resp_len = recvmsg(fd, msg, 0);

    if (resp_len == 0)
        die("EOF on netlink\n");

    if (resp_len < 0)
        die("netlink receive error: %m\n");

    return resp_len;
}

/**
 * @brief Проверяет ответ Netlink сокета на ошибки.
 * 
 * @param sock_fd Дескриптор Netlink сокета
 * @note Вызывает die() при обнаружении ошибки.
 */
static void check_response(int sock_fd)
{
    struct iovec iov;
    struct msghdr msg = {
            .msg_name = NULL,
            .msg_namelen = 0,
            .msg_iov = &iov,
            .msg_iovlen = 1
    };
    char *resp = malloc(MAX_PAYLOAD);

    ssize_t resp_len = read_response(sock_fd, &msg, &resp);

    struct nlmsghdr *hdr = (struct nlmsghdr *) resp;
    int nlmsglen = hdr->nlmsg_len;
    int datalen = nlmsglen - sizeof(*hdr);

    if (datalen < 0 || nlmsglen > resp_len) {
        if (msg.msg_flags & MSG_TRUNC)
            die("received truncated message\n");

        die("malformed message: nlmsg_len=%d\n", nlmsglen);
    }

    if (hdr->nlmsg_type == NLMSG_ERROR) {
        struct nlmsgerr *err = (struct nlmsgerr *) NLMSG_DATA(hdr);

        if (datalen < sizeof(struct nlmsgerr))
            fprintf(stderr, "ERROR truncated!\n");

        if(err->error) {
            errno = -err->error;
            die("RTNETLINK: %m\n");
        }
    }

    free(resp);
}

/**
 * @brief Создаёт сетевой сокет с указанными параметрами.
 * 
 * @param domain Домен сокета (например, PF_INET, AF_NETLINK)
 * @param type Тип сокета (например, SOCK_DGRAM, SOCK_RAW)
 * @param protocol Протокол (например, IPPROTO_IP, NETLINK_ROUTE)
 * @return Дескриптор созданного сокета
 * @note Завершает программу при ошибке создания сокета.
 */
int create_socket(int domain, int type, int protocol)
{
    int sock_fd = socket(domain, type, protocol);
    if (sock_fd < 0)
        die("cannot open socket: %m\n");

    return sock_fd;
}

/**
 * @brief Отправляет Netlink сообщение и проверяет ответ.
 * 
 * @param sock_fd Дескриптор Netlink сокета
 * @param n Указатель на Netlink сообщение, готовое к отправке
 */
static void send_nlmsg(int sock_fd, struct nlmsghdr *n)
{
    struct iovec iov = {
            .iov_base = n,
            .iov_len = n->nlmsg_len
    };

    struct msghdr msg = {
            .msg_name = NULL,
            .msg_namelen = 0,
            .msg_iov = &iov,
            .msg_iovlen = 1
    };

    n->nlmsg_seq++;

    ssize_t status = sendmsg(sock_fd, &msg, 0);
    if (status < 0)
        die("cannot talk to rtnetlink: %m\n");

    check_response(sock_fd);
}

/**
 * @brief Получает дескриптор сетевого пространства имен процесса по его PID.
 * 
 * @param pid Идентификатор процесса
 * @return Дескриптор сетевого пространства имен процесса
 * @note Завершает программу при ошибке открытия файла netns.
 */
int get_netns_fd(int pid)
{
    char path[256];
    sprintf(path, "/proc/%d/ns/net", pid);

    int fd = open(path, O_RDONLY);

    if (fd < 0)
        die("cannot read netns file %s: %m\n", path);

    return fd;
}

/**
 * @brief Активирует сетевой интерфейс, назначая IP и маску.
 * 
 * @param ifname Имя интерфейса (например, "eth0")
 * @param ip IP-адрес в виде строки (например, "192.168.1.10")
 * @param netmask Маска подсети в виде строки (например, "255.255.255.0")
 */
void if_up(
        char *ifname, char *ip, char *netmask)
{
    int sock_fd = create_socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(struct sockaddr_in));
    saddr.sin_family = AF_INET;
    saddr.sin_port = 0;

    char *p = (char *) &saddr;

    saddr.sin_addr.s_addr = inet_addr(ip);
    memcpy(&ifr.ifr_addr, p, sizeof(struct sockaddr));
    if (ioctl(sock_fd, SIOCSIFADDR, &ifr))
        die("cannot set ip addr %s, %s: %m\n", ifname, ip);

    saddr.sin_addr.s_addr = inet_addr(netmask);
    memcpy(&ifr.ifr_addr, p, sizeof(struct sockaddr));
    if (ioctl(sock_fd, SIOCSIFNETMASK, &ifr))
        die("cannot set netmask for addr %s, %s: %m\n", ifname, netmask);

    ifr.ifr_flags |= IFF_UP | IFF_BROADCAST |
                     IFF_RUNNING | IFF_MULTICAST;
    if (ioctl(sock_fd, SIOCSIFFLAGS, &ifr))
        die("cannot set flags for addr %s, %s: %m\n", ifname, ip);

    close(sock_fd);
}

/**
 * @brief Создает пару виртуальных Ethernet интерфейсов (veth pair) с заданными именами.
 * 
 * @param sock_fd Дескриптор Netlink сокета
 * @param ifname Имя первой стороны veth (например, "veth0")
 * @param peername Имя второй стороны veth (например, "veth1")
 */
void create_veth(int sock_fd, char *ifname, char *peername)
{
    // Формируем Netlink запрос на создание link
    __u16 flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
    struct nl_req req = {
            .n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
            .n.nlmsg_flags = flags,
            .n.nlmsg_type = RTM_NEWLINK,
            .i.ifi_family = PF_NETLINK,
    };
    struct nlmsghdr *n = &req.n;
    int maxlen = sizeof(req);

    addattr_l(n, maxlen, IFLA_IFNAME, ifname, strlen(ifname) + 1);

    struct rtattr *linfo = addattr_nest(n, maxlen, IFLA_LINKINFO);
    addattr_l(&req.n, sizeof(req), IFLA_INFO_KIND, "veth", 5);

    struct rtattr *linfodata = addattr_nest(n, maxlen, IFLA_INFO_DATA);

    struct rtattr *peerinfo = addattr_nest(n, maxlen, VETH_INFO_PEER);
    n->nlmsg_len += sizeof(struct ifinfomsg);
    addattr_l(n, maxlen, IFLA_IFNAME, peername, strlen(peername) + 1);
    addattr_nest_end(n, peerinfo);

    addattr_nest_end(n, linfodata);
    addattr_nest_end(n, linfo);

    send_nlmsg(sock_fd, n);
}

/**
 * @brief Перемещает сетевой интерфейс в сетевое пространство имен процесса.
 * 
 * @param sock_fd Дескриптор Netlink сокета
 * @param ifname Имя интерфейса для перемещения
 * @param netns Файловый дескриптор сетевого пространства имен назначения
 */
void move_if_to_pid_netns(int sock_fd, char *ifname, int netns)
{
    struct nl_req req = {
            .n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
            .n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
            .n.nlmsg_type = RTM_NEWLINK,
            .i.ifi_family = PF_NETLINK,
    };

    addattr_l(&req.n, sizeof(req), IFLA_NET_NS_FD, &netns, sizeof(netns));
    addattr_l(&req.n, sizeof(req), IFLA_IFNAME, ifname, strlen(ifname) + 1);
    send_nlmsg(sock_fd, &req.n);
}
