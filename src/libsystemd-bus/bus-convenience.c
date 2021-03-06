/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  This file is part of systemd.

  Copyright 2013 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include "bus-internal.h"
#include "bus-message.h"
#include "bus-signature.h"
#include "bus-util.h"

int sd_bus_emit_signal(
                sd_bus *bus,
                const char *path,
                const char *interface,
                const char *member,
                const char *types, ...) {

        _cleanup_bus_message_unref_ sd_bus_message *m = NULL;
        int r;

        assert_return(bus, -EINVAL);
        assert_return(BUS_IS_OPEN(bus->state), -ENOTCONN);
        assert_return(!bus_pid_changed(bus), -ECHILD);

        r = sd_bus_message_new_signal(bus, path, interface, member, &m);
        if (r < 0)
                return r;

        if (!isempty(types)) {
                va_list ap;

                va_start(ap, types);
                r = bus_message_append_ap(m, types, ap);
                va_end(ap);
                if (r < 0)
                        return r;
        }

        return sd_bus_send(bus, m, NULL);
}

int sd_bus_call_method(
                sd_bus *bus,
                const char *destination,
                const char *path,
                const char *interface,
                const char *member,
                sd_bus_error *error,
                sd_bus_message **reply,
                const char *types, ...) {

        _cleanup_bus_message_unref_ sd_bus_message *m = NULL;
        int r;

        assert_return(bus, -EINVAL);
        assert_return(BUS_IS_OPEN(bus->state), -ENOTCONN);
        assert_return(!bus_pid_changed(bus), -ECHILD);

        r = sd_bus_message_new_method_call(bus, destination, path, interface, member, &m);
        if (r < 0)
                return r;

        if (!isempty(types)) {
                va_list ap;

                va_start(ap, types);
                r = bus_message_append_ap(m, types, ap);
                va_end(ap);
                if (r < 0)
                        return r;
        }

        return sd_bus_send_with_reply_and_block(bus, m, 0, error, reply);
}

int sd_bus_reply_method_return(
                sd_bus *bus,
                sd_bus_message *call,
                const char *types, ...) {

        _cleanup_bus_message_unref_ sd_bus_message *m = NULL;
        int r;

        assert_return(bus, -EINVAL);
        assert_return(call, -EINVAL);
        assert_return(call->sealed, -EPERM);
        assert_return(call->header->type == SD_BUS_MESSAGE_METHOD_CALL, -EINVAL);
        assert_return(BUS_IS_OPEN(bus->state), -ENOTCONN);
        assert_return(!bus_pid_changed(bus), -ECHILD);

        if (call->header->flags & SD_BUS_MESSAGE_NO_REPLY_EXPECTED)
                return 0;

        r = sd_bus_message_new_method_return(bus, call, &m);
        if (r < 0)
                return r;

        if (!isempty(types)) {
                va_list ap;

                va_start(ap, types);
                r = bus_message_append_ap(m, types, ap);
                va_end(ap);
                if (r < 0)
                        return r;
        }

        return sd_bus_send(bus, m, NULL);
}

int sd_bus_reply_method_error(
                sd_bus *bus,
                sd_bus_message *call,
                const sd_bus_error *e) {

        _cleanup_bus_message_unref_ sd_bus_message *m = NULL;
        int r;

        assert_return(bus, -EINVAL);
        assert_return(call, -EINVAL);
        assert_return(call->sealed, -EPERM);
        assert_return(call->header->type == SD_BUS_MESSAGE_METHOD_CALL, -EINVAL);
        assert_return(sd_bus_error_is_set(e), -EINVAL);
        assert_return(BUS_IS_OPEN(bus->state), -ENOTCONN);
        assert_return(!bus_pid_changed(bus), -ECHILD);

        if (call->header->flags & SD_BUS_MESSAGE_NO_REPLY_EXPECTED)
                return 0;

        r = sd_bus_message_new_method_error(bus, call, e, &m);
        if (r < 0)
                return r;

        return sd_bus_send(bus, m, NULL);
}

int sd_bus_reply_method_errorf(
                sd_bus *bus,
                sd_bus_message *call,
                const char *name,
                const char *format,
                ...) {

        _cleanup_bus_error_free_ sd_bus_error error = SD_BUS_ERROR_NULL;
        va_list ap;
        int r;

        assert_return(bus, -EINVAL);
        assert_return(call, -EINVAL);
        assert_return(call->sealed, -EPERM);
        assert_return(call->header->type == SD_BUS_MESSAGE_METHOD_CALL, -EINVAL);
        assert_return(BUS_IS_OPEN(bus->state), -ENOTCONN);
        assert_return(!bus_pid_changed(bus), -ECHILD);

        if (call->header->flags & SD_BUS_MESSAGE_NO_REPLY_EXPECTED)
                return 0;

        va_start(ap, format);
        r = bus_error_setfv(&error, name, format, ap);
        va_end(ap);

        if (r < 0)
                return r;

        return sd_bus_reply_method_error(bus, call, &error);
}

int sd_bus_reply_method_errno(
                sd_bus *bus,
                sd_bus_message *call,
                int error,
                const sd_bus_error *p) {

        _cleanup_bus_error_free_ sd_bus_error berror = SD_BUS_ERROR_NULL;

        assert_return(bus, -EINVAL);
        assert_return(call, -EINVAL);
        assert_return(call->sealed, -EPERM);
        assert_return(call->header->type == SD_BUS_MESSAGE_METHOD_CALL, -EINVAL);
        assert_return(BUS_IS_OPEN(bus->state), -ENOTCONN);
        assert_return(!bus_pid_changed(bus), -ECHILD);

        if (call->header->flags & SD_BUS_MESSAGE_NO_REPLY_EXPECTED)
                return 0;

        if (sd_bus_error_is_set(p))
                return sd_bus_reply_method_error(bus, call, p);

        sd_bus_error_set_errno(&berror, error);

        return sd_bus_reply_method_error(bus, call, &berror);
}

int sd_bus_reply_method_errnof(
                sd_bus *bus,
                sd_bus_message *call,
                int error,
                const char *format,
                ...) {

        _cleanup_bus_error_free_ sd_bus_error berror = SD_BUS_ERROR_NULL;
        va_list ap;

        assert_return(bus, -EINVAL);
        assert_return(call, -EINVAL);
        assert_return(call->sealed, -EPERM);
        assert_return(call->header->type == SD_BUS_MESSAGE_METHOD_CALL, -EINVAL);
        assert_return(BUS_IS_OPEN(bus->state), -ENOTCONN);
        assert_return(!bus_pid_changed(bus), -ECHILD);

        if (call->header->flags & SD_BUS_MESSAGE_NO_REPLY_EXPECTED)
                return 0;

        va_start(ap, format);
        bus_error_set_errnofv(&berror, error, format, ap);
        va_end(ap);

        return sd_bus_reply_method_error(bus, call, &berror);
}

int sd_bus_get_property(
                sd_bus *bus,
                const char *destination,
                const char *path,
                const char *interface,
                const char *member,
                sd_bus_error *error,
                sd_bus_message **reply,
                const char *type) {

        sd_bus_message *rep = NULL;
        int r;

        assert_return(bus, -EINVAL);
        assert_return(isempty(interface) || interface_name_is_valid(interface), -EINVAL);
        assert_return(member_name_is_valid(member), -EINVAL);
        assert_return(reply, -EINVAL);
        assert_return(signature_is_single(type, false), -EINVAL);
        assert_return(BUS_IS_OPEN(bus->state), -ENOTCONN);
        assert_return(!bus_pid_changed(bus), -ECHILD);

        r = sd_bus_call_method(bus, destination, path, "org.freedesktop.DBus.Properties", "Get", error, &rep, "ss", strempty(interface), member);
        if (r < 0)
                return r;

        r = sd_bus_message_enter_container(rep, 'v', type);
        if (r < 0) {
                sd_bus_message_unref(rep);
                return r;
        }

        *reply = rep;
        return 0;
}

int sd_bus_set_property(
                sd_bus *bus,
                const char *destination,
                const char *path,
                const char *interface,
                const char *member,
                sd_bus_error *error,
                const char *type, ...) {

        _cleanup_bus_message_unref_ sd_bus_message *m = NULL;
        va_list ap;
        int r;

        assert_return(bus, -EINVAL);
        assert_return(isempty(interface) || interface_name_is_valid(interface), -EINVAL);
        assert_return(member_name_is_valid(member), -EINVAL);
        assert_return(signature_is_single(type, false), -EINVAL);
        assert_return(BUS_IS_OPEN(bus->state), -ENOTCONN);
        assert_return(!bus_pid_changed(bus), -ECHILD);

        r = sd_bus_message_new_method_call(bus, destination, path, "org.freedesktop.DBus.Properties", "Set", &m);
        if (r < 0)
                return r;

        r = sd_bus_message_append(m, "ss", strempty(interface), member);
        if (r < 0)
                return r;

        r = sd_bus_message_open_container(m, 'v', type);
        if (r < 0)
                return r;

        va_start(ap, type);
        r = bus_message_append_ap(m, type, ap);
        va_end(ap);
        if (r < 0)
                return r;

        r = sd_bus_message_close_container(m);
        if (r < 0)
                return r;

        return sd_bus_send_with_reply_and_block(bus, m, 0, error, NULL);
}
