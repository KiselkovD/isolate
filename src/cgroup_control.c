#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define CGROUP_BASE "/sys/fs/cgroup"
#define CGROUP_NAME "isolate_group"
#define CGROUP_PATH CGROUP_BASE "/" CGROUP_NAME

/**
 * @brief Записывает строку в файл, с проверкой ошибок
 *
 * @param path Путь к файлу
 * @param value Строка для записи
 */
static void write_to_file(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    if (write(fd, value, strlen(value)) < 0) {
        perror("write");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
}

/**
 * @brief Создаёт cgroup директорию, если отсутствует
 */
static void create_cgroup_directory()
{
    struct stat st;
    if (stat(CGROUP_PATH, &st) == -1) {
        if (mkdir(CGROUP_PATH, 0755) == -1) {
            perror("mkdir cgroup");
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * @brief Устанавливает лимит CPU (cpu.max) в cgroup
 * @param max_us Ограничение процессорного времени в микросекундах (например "20000 100000" для 20%)
 */
void cgroup_set_cpu_limit(const char *max_us)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/cpu.max", CGROUP_PATH);
    write_to_file(path, max_us);
}

/**
 * @brief Устанавливает лимит памяти (memory.max) в cgroup
 * @param max_bytes Размер памяти с суффиксом (например "50M", "52428800")
 */
void cgroup_set_memory_limit(const char *max_bytes)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/memory.max", CGROUP_PATH);
    write_to_file(path, max_bytes);
}

/**
 * @brief Ограничивает количество процессов в cgroup (pids.max)
 * @param max_pids Максимальное количество процессов (например "50")
 */
void cgroup_set_pids_limit(const char *max_pids)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/pids.max", CGROUP_PATH);
    write_to_file(path, max_pids);
}

/**
 * @brief Добавляет процесс с pid в cgroup (cgroup.procs)
 * @param pid Идентификатор процесса
 */
void cgroup_add_process(pid_t pid)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/cgroup.procs", CGROUP_PATH);
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    write_to_file(path, pid_str);
}

/**
 * @brief Инициализирует cgroup и применяет все установленные ограничения к процессу
 *
 * @param pid PID процесса, для которого выделяется cgroup
 */
void cgroup_init_and_limit(pid_t pid)
{
    create_cgroup_directory();

    // Пример лимитов
    cgroup_set_cpu_limit("20000 100000");  // 20% CPU
    cgroup_set_memory_limit("50M");        // 50 МБ памяти
    cgroup_set_pids_limit("50");           // Максимум 50 процессов

    cgroup_add_process(pid);
}
