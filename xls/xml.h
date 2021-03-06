/*
 * xml.h
 *
 * Copyright © 2017 Oliver Adams
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
 */

#ifndef CPPDATALIB_XML_XLS_H
#define CPPDATALIB_XML_XLS_H

#include "../core/core.h"

namespace cppdatalib
{
    namespace xml_xls
    {
        namespace impl
        {
            class stream_writer_base : public core::stream_handler, public core::stream_writer
            {
            public:
                stream_writer_base(core::ostream_handle &stream) : core::stream_writer(stream) {}

            protected:
                core::ostream &write_string(core::ostream &stream, const std::string &str)
                {
                    for (size_t i = 0; i < str.size(); ++i)
                    {
                        int c = str[i] & 0xff;

                        switch (c)
                        {
                            case '"': stream.write("&quot;", 6); break;
                            case '&': stream.write("&amp;", 5); break;
                            case '\'': stream.write("&apos;", 6); break;
                            case '<': stream.write("&lt;", 4); break;
                            case '>': stream.write("&gt;", 4); break;
                            default:
                                if (iscntrl(c))
                                    stream.write("&#", 2).put(c).put(';');
                                else
                                    stream.put(str[i]);
                                break;
                        }
                    }

                    return stream;
                }
            };
        }

        class table_writer : public impl::stream_writer_base
        {
        public:
            table_writer(core::ostream_handle output) : impl::stream_writer_base(output) {}

        protected:
            void begin_() {stream() << "<Table>"; stream().precision(CPPDATALIB_REAL_DIG);}
            void end_() {stream() << "</Table>";}

            void begin_item_(const core::value &v)
            {
                const char *type = NULL;

                switch (v.get_type())
                {
                    case core::null: type = "String"; break;
                    case core::boolean: type = "Boolean"; break;
                    case core::integer:
                    case core::uinteger:
                    case core::real: type = "Number"; break;
                    case core::string:
                        if (v.get_subtype() == core::date ||
                            v.get_subtype() == core::time ||
                            v.get_subtype() == core::datetime)
                            type = "DateTime";
                        else
                            type = "String";
                        break;
                    case core::array:
                        if (nesting_depth() == 0)
                            break; // No need to do anything for the top-level array
                        stream() << "<Row>";
                        break;
                    default: break;
                }

                if (type)
                    stream() << "<Cell><Data ss:Type=\"" << type << "\">";
            }
            void end_item_(const core::value &v)
            {
                if (v.is_array())
                {
                    if (nesting_depth() > 1)
                        stream() << "</Row>";
                }
                else
                    stream() << "</Data></Cell>";
            }

            void bool_(const core::value &v) {stream() << v.as_int();}
            void integer_(const core::value &v) {stream() << v.get_int_unchecked();}
            void uinteger_(const core::value &v) {stream() << v.get_uint_unchecked();}
            void real_(const core::value &v) {stream() << v.get_real_unchecked();}
            void string_data_(const core::value &v, bool) {write_string(stream(), v.get_string_unchecked());}

            void begin_array_(const core::value &, core::int_t, bool)
            {
                if (nesting_depth() == 2)
                    throw core::error("XML XLS - 'array' value not allowed in row output");
            }
            void begin_object_(const core::value &, core::int_t, bool) {throw core::error("XML XLS - 'object' value not allowed in output");}
        };

        class worksheet_writer : public table_writer
        {
            std::string worksheet_name;

        public:
            worksheet_writer(core::ostream &output, const std::string &worksheet_name) : table_writer(output), worksheet_name(worksheet_name) {}

        protected:
            void begin_()
            {
                if (worksheet_name.find_first_of("\\/?*[]") != std::string::npos)
                    throw core::error("XML XLS - Invalid worksheet name cannot contain any of '\\/?*[]'");

                stream() << "<Worksheet ss:Name=\"";
                write_string(stream(), worksheet_name);
                stream() << "\">";

                table_writer::begin_();
            }
            void end_()
            {
                table_writer::end_();

                stream() << "</Worksheet>";
            }
        };

        class workbook_writer : public worksheet_writer
        {
        public:
            workbook_writer(core::ostream &output, const std::string &worksheet_name) : worksheet_writer(output, worksheet_name) {}

        protected:
            void begin_()
            {
                stream() << "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\""
                              << " xmlns:c=\"urn:schemas-microsoft-com:office:component:spreadsheet\""
                              << " xmlns:html=\"http://www.w3.org/TR/REC-html40\""
                              << " xmlns:o=\"urn:schemas-microsoft-com:office:office\""
                              << " xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\""
                              << " xmlns:x2=\"http://schemas.microsoft.com/office/excel/2003/xml\""
                              << " xmlns:x=\"urn:schemas-microsoft-com:office:excel\""
                              << " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">";

                worksheet_writer::begin_();
            }
            void end_()
            {
                worksheet_writer::end_();

                stream() << "</Workbook>";
            }
        };

        class document_writer : public workbook_writer
        {
        public:
            document_writer(core::ostream &output, const std::string &worksheet_name) : workbook_writer(output, worksheet_name) {}

        protected:
            void begin_()
            {
                stream() << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
                              << "<?mso-application progid=\"Excel.Sheet\"?>";

                workbook_writer::begin_();
            }
        };

        inline std::string to_xml_xls_table(const core::value &v)
        {
            core::ostringstream stream;
            table_writer writer(stream);
            writer << v;
            return stream.str();
        }

        inline std::string to_xml_xls_worksheet(const core::value &v, const std::string &worksheet_name)
        {
            core::ostringstream stream;
            worksheet_writer writer(stream, worksheet_name);
            writer << v;
            return stream.str();
        }

        inline std::string to_xml_xls_workbook(const core::value &v, const std::string &worksheet_name)
        {
            core::ostringstream stream;
            workbook_writer writer(stream, worksheet_name);
            writer << v;
            return stream.str();
        }

        inline std::string to_xml_xls_document(const core::value &v, const std::string &worksheet_name)
        {
            core::ostringstream stream;
            document_writer writer(stream, worksheet_name);
            writer << v;
            return stream.str();
        }

        inline std::string to_xml_xls(const core::value &v, const std::string &worksheet_name) {return to_xml_xls_document(v, worksheet_name);}
    }
}

#endif // CPPDATALIB_XML_XLS_H
