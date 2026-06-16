/*
 * test_audit_kernel_events.c - observe kernel audit-style inotify events
 *
 * This test intentionally bypasses Alfred's runtime. It uses inotify directly
 * to record what the Linux kernel emits for a simple read-only file session
 * when a directory watch subscribes to IN_OPEN, IN_ACCESS, and
 * IN_CLOSE_NOWRITE.
 *
 * Expected kernel event contract:
 *
 * raw inotify queue:
 * - IN_OPEN          name=audit-read.txt
 * - IN_ACCESS        name=audit-read.txt
 * - IN_CLOSE_NOWRITE name=audit-read.txt
 *
 * Alfred raw/core:
 * - none; these events are not enabled by Alfred's runtime in this branch
 *
 * Meaning:
 * These events are real kernel facts, but they describe audit/read activity,
 * not filesystem mutation. The test fixes the kernel behavior before Alfred
 * decides whether to expose a future audit stream behind explicit
 * configuration.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#define EVENT_BUF_SIZE (16 * (sizeof(struct inotify_event) + NAME_MAX + 1))

typedef struct audit_seen {
    int open;
    int access;
    int close_nowrite;
    int close_write;
} audit_seen_t;

static void remember_event(const struct inotify_event *ev,
                           const char *expected_name,
                           audit_seen_t *seen)
{
    if (ev == NULL || expected_name == NULL || seen == NULL)
        return;

    if (ev->len == 0 || strcmp(ev->name, expected_name) != 0)
        return;

    if ((ev->mask & IN_OPEN) != 0)
        seen->open = 1;

    if ((ev->mask & IN_ACCESS) != 0)
        seen->access = 1;

    if ((ev->mask & IN_CLOSE_NOWRITE) != 0)
        seen->close_nowrite = 1;

    if ((ev->mask & IN_CLOSE_WRITE) != 0)
        seen->close_write = 1;
}

static void drain_events(int inotify_fd,
                         const char *expected_name,
                         audit_seen_t *seen)
{
    char buffer[EVENT_BUF_SIZE];

    for (;;) {
        ssize_t nread = read(inotify_fd, buffer, sizeof(buffer));

        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;

            perror("read inotify");
            assert(0);
        }

        size_t offset = 0;

        while (offset < (size_t)nread) {
            const struct inotify_event *ev =
                (const struct inotify_event *)(const void *)(buffer + offset);

            remember_event(ev, expected_name, seen);

            offset += sizeof(*ev) + ev->len;
        }
    }
}

static void wait_and_drain_events(int inotify_fd,
                                  const char *expected_name,
                                  audit_seen_t *seen)
{
    struct pollfd pfd;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = inotify_fd;
    pfd.events = POLLIN;

    int ready = poll(&pfd, 1, 1000);

    assert(ready > 0);
    assert((pfd.revents & POLLIN) != 0);

    drain_events(inotify_fd, expected_name, seen);
}

int main(void)
{
    const char *root = "/tmp/alfred_audit_kernel_events";
    const char *name = "audit-read.txt";
    char path[512];

    snprintf(path, sizeof(path), "%s/%s", root, name);

    unlink(path);
    rmdir(root);

    assert(mkdir(root, 0700) == 0);

    int write_fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    assert(write_fd >= 0);
    assert(write(write_fd, "audit\n", 6) == 6);
    assert(close(write_fd) == 0);

    int inotify_fd = inotify_init1(IN_NONBLOCK);
    assert(inotify_fd >= 0);

    int wd = inotify_add_watch(inotify_fd,
                               root,
                               IN_OPEN | IN_ACCESS | IN_CLOSE_NOWRITE);
    assert(wd >= 0);

    int read_fd = open(path, O_RDONLY);
    assert(read_fd >= 0);

    char byte;
    assert(read(read_fd, &byte, sizeof(byte)) == 1);
    assert(close(read_fd) == 0);

    audit_seen_t seen;

    memset(&seen, 0, sizeof(seen));

    wait_and_drain_events(inotify_fd, name, &seen);

    assert(seen.open);
    assert(seen.access);
    assert(seen.close_nowrite);
    assert(!seen.close_write);

    assert(inotify_rm_watch(inotify_fd, wd) == 0);
    assert(close(inotify_fd) == 0);

    unlink(path);
    rmdir(root);

    return 0;
}
