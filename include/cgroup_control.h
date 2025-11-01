#ifndef CGROUP_CONTROL_H
#define CGROUP_CONTROL_H

#include <sys/types.h>

/**
 * @brief Создаёт директорию cgroup для проекта (если отсутствует)
 *
 * Каталог создаётся по пути /sys/fs/cgroup/isolate_group.
 * Вызывает ошибку и завершает программу при неудаче.
 */
void cgroup_create_directory(void);

/**
 * @brief Устанавливает лимит CPU в микросекундах и период для cpu.max
 *
 * @param max_quota Ограничение времени использования CPU (например "20000 100000" для 20%)
 */
void cgroup_set_cpu_limit(const char *max_quota);

/**
 * @brief Устанавливает ограничение памяти через memory.max
 *
 * @param max_value Строка с пределом памяти (например "50000000" или "50M")
 */
void cgroup_set_memory_limit(const char *max_value);

/**
 * @brief Устанавливает ограничения по I/O вводу-выводу через io.max
 *
 * @param io_limits Строка с ограничениями, зависит от контроллера
 */
void cgroup_set_io_limit(const char *io_limits);

/**
 * @brief Устанавливает лимит количества PIDs через pids.max
 *
 * @param max_pids Максимальное число процессов (например "50")
 */
void cgroup_set_pids_limit(const char *max_pids);

/**
 * @brief Добавляет процесс с указанным PID в cgroup
 *
 * @param pid Идентификатор процесса, который нужно добавить
 */
void cgroup_add_process(pid_t pid);

/**
 * @brief Инициализирует cgroup и задаёт стандартные лимиты для указанного PID
 *
 * @param pid PID процесса для добавления в cgroup
 */
void cgroup_init_and_limit(pid_t pid);

#endif // CGROUP_CONTROL_H
