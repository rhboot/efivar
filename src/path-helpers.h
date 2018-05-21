/*
 * path-helper.h
 * Copyright 2018 Peter Jones <pjones@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef PATH_HELPER_H_
#define PATH_HELPER_H_

void HIDDEN fill_spans(const char *str, const char *reject, void *spanbuf);
unsigned int HIDDEN count_spans(const char *str, const char *reject, unsigned int *chars);
int HIDDEN find_path_segment(const char *path, int segment, const char **pos, size_t *len);

#define pathseg(path, seg)                                              \
        ({                                                              \
                const char *pos_ = NULL;                                \
                char *ret_ = NULL;                                      \
                size_t len_ = 0;                                        \
                int rc_;                                                \
                                                                        \
                rc_ = find_path_segment(path, seg, &pos_, &len_);       \
                if (rc_ >= 0) {                                         \
                        ret_ = alloca(len_ + 1);                        \
                        if (ret_) {                                     \
                                memcpy(ret_, pos_, len_);               \
                                ret_[len_] = '\0';                      \
                        }                                               \
                }                                                       \
                ret_;                                                   \
        })



#endif /* !PATH_HELPER_H_ */
// vim:fenc=utf-8:tw=75:et
