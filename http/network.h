/*
 * network.h
 *
 * Copyright © 2018 Oliver Adams
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Disclaimer:
 * Trademarked product names referred to in this file are the property of their
 * respective owners. These trademark owners are not affiliated with the author
 * or copyright holder(s) of this file in any capacity, and do not endorse this
 * software nor the authorship and existence of this file.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "../core/core.h"

#ifdef CPPDATALIB_ENABLE_QT_NETWORK
#include <QtNetwork/QtNetwork>
#endif

namespace cppdatalib
{
    namespace http
    {
#ifdef CPPDATALIB_ENABLE_QT_NETWORK
        class qt_parser : public core::stream_input
        {
            core::value url, headers;
            std::string verb;
            QNetworkAccessManager *manager;
            bool owns_manager;

            QNetworkRequest request;
            QNetworkReply *reply;

        public:
            qt_parser(const core::value &url /* URL may include headers if CPPDATALIB_ENABLE_ATTRIBUTES is defined */,
                      const std::string &verb = "GET",
                      const core::object_t &headers = core::object_t(),
                      QNetworkAccessManager *manager = nullptr)
                : url(url)
                , headers(headers)
                , verb(verb)
                , manager(manager? manager: new QNetworkAccessManager())
                , owns_manager(manager == nullptr)
                , reply(nullptr)
            {
                reset();
            }
            ~qt_parser()
            {
                if (reply)
                    reply->deleteLater();
                if (owns_manager)
                    delete manager;
            }

        protected:
            void reset_()
            {
                request.setUrl(QUrl(QString::fromStdString(url.as_string())));
#ifdef CPPDATALIB_ENABLE_ATTRIBUTES
                for (const auto &header: url.get_attributes())
                    request.setRawHeader(QByteArray::fromStdString(header.first.as_string()),
                                         QByteArray::fromStdString(header.second.as_string()));
#endif
                for (const auto &header: headers.get_object_unchecked())
                    request.setRawHeader(QByteArray::fromStdString(header.first.as_string()),
                                         QByteArray::fromStdString(header.second.as_string()));
                request.setMaximumRedirectsAllowed(20);
                request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

                if (reply)
                    reply->deleteLater();
                reply = nullptr;
            }

            void write_one_()
            {
#ifndef CPPDATALIB_EVENT_LOOP_CALLBACK
                qApp->processEvents();
#endif

                if (was_just_reset())
                {
                    reply = manager->sendCustomRequest(request, QByteArray::fromStdString(verb));

                    get_output()->begin_string(core::value(core::string_t(), core::blob), core::stream_handler::unknown_size);
                    return;
                }

                if (reply->error() != QNetworkReply::NoError)
                    throw core::custom_error("HTTP - " + reply->errorString().toStdString());

                if (reply->isFinished())
                {
                    if (reply->bytesAvailable() > 0)
                        get_output()->append_to_string(core::value(reply->readAll().toStdString()));
                    get_output()->end_string(core::value(core::string_t(), core::blob));

                    reply->deleteLater(); reply = nullptr;
                }
                else if (reply->bytesAvailable() > 0)
                    get_output()->append_to_string(core::value(reply->readAll().toStdString()));
            }
        };
#endif

        class parser : public core::stream_input
        {
            core::value url;
            core::object_t headers;
            std::string verb;
            core::network_library interface;
            core::stream_input *interface_stream;
            void *context;

        public:
            parser(const core::value &url /* URL may include headers if CPPDATALIB_ENABLE_ATTRIBUTES is defined */,
                   core::network_library interface = core::default_network_library,
                   const std::string &verb = "GET",
                   const core::object_t &headers = core::object_t(),
                   void *context = nullptr /* Context varies, based on individual interfaces (e.g. qt_parser takes a pointer to a QNetworkAccessManager) */)
                : url(url)
                , headers(headers)
                , verb(verb)
                , interface(core::unknown_network_library)
                , interface_stream(nullptr)
                , context(context)
            {
                set_interface(interface);
                reset();
            }
            ~parser()
            {
                delete interface_stream;
            }

            void set_interface(core::network_library interface)
            {
                if (this->interface != interface)
                {
                    this->interface = interface;
                    delete interface_stream;

                    if (interface >= core::network_library_count)
                    {
                        interface_stream = nullptr;
                        return;
                    }

                    switch (interface)
                    {
                        case core::unknown_network_library:
                        default:
                            interface_stream = nullptr;
                            break;
#ifdef CPPDATALIB_ENABLE_QT_NETWORK
                        case core::qt_network_library:
                            interface_stream = new qt_parser(url, verb, headers, static_cast<QNetworkAccessManager *>(context));
                            if (get_output())
                                interface_stream->set_output(*get_output());
                            break;
#endif
                    }

                    reset();
                }
            }
            core::network_library get_interface() const {return interface;}

        protected:
            void test_interface()
            {
                if (interface_stream == nullptr)
                    throw core::error("HTTP - invalid, non-existent, or disabled network interface selected");
            }

            void output_changed_()
            {
                if (interface_stream != nullptr)
                    interface_stream->set_output(*get_output());
            }

            void reset_()
            {
                if (interface_stream != nullptr)
                    interface_stream->reset();
            }

            void write_one_()
            {
                test_interface();
                interface_stream->write_one();
            }
        };
    }
}

#endif // NETWORK_H