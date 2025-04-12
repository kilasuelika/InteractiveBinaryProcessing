export module LibIBP.Interpreter;

import std;
import LibIBP.Common;
import <boost/iostreams/device/mapped_file.hpp>;
import <exprtk.hpp>;
import <fmt/format.h>;
import <fmt/ranges.h>;
import <fmt/color.h>;

export namespace IBP
{
    class ExpressionEvaluator {
    private:
        typedef double T;
        typedef exprtk::symbol_table<T> symbol_table_t;
        typedef exprtk::expression<T> expression_t;
        typedef exprtk::parser<T> parser_t;

        symbol_table_t symbol_table_;
        expression_t expression_;
        parser_t parser_;
        std::vector<std::unique_ptr<exprtk::ifunction<T>>> function_adapters_;

    public:
        ExpressionEvaluator() {
            symbol_table_.add_constants();
            expression_.register_symbol_table(symbol_table_);
        }

        void add_variable(const std::string& var, double& v)
        {
            symbol_table_.add_variable(var, v);
        }

        template <typename Function>
        void register_function(const std::string& name, Function func, int num_params) {
            if (num_params == 1) {
                struct Function1 : public exprtk::ifunction<T> {
                    Function func_;

                    Function1(Function f) : exprtk::ifunction<T>(1), func_(f) {}

                    T operator()(const T& v1) override {
                        const T args[1] = { v1 };
                        return func_(args);
                    }
                };
                auto adapter = new Function1(func);
                function_adapters_.emplace_back(adapter);
                symbol_table_.add_function(name, *adapter);
            }
            else if (num_params == 2) {
                struct Function2 : public exprtk::ifunction<T> {
                    Function func_;

                    Function2(Function f) : exprtk::ifunction<T>(2), func_(f) {}

                    T operator()(const T& v1, const T& v2) override {
                        const T args[2] = { v1, v2 };
                        return func_(args);
                    }
                };
                auto adapter = new Function2(func);
                function_adapters_.emplace_back(adapter);
                symbol_table_.add_function(name, *adapter);
            }
            else if (num_params == 3) {
                struct Function3 : public exprtk::ifunction<T> {
                    Function func_;

                    Function3(Function f) : exprtk::ifunction<T>(3), func_(f) {}

                    T operator()(const T& v1, const T& v2, const T& v3) override {
                        const T args[3] = { v1, v2, v3 };
                        return func_(args);
                    }
                };
                auto adapter = new Function3(func);
                function_adapters_.emplace_back(adapter);
                symbol_table_.add_function(name, *adapter);
            }
        }

        std::optional<T> evaluate(const std::string& expr) {
            if (!parser_.compile(expr, expression_)) {
                std::cerr << "Error: " << parser_.error() << std::endl;
                return {};
            }
            return expression_.value();
        }
    };


    struct TypeExpression {
        std::string type;
        std::optional<std::variant<int, std::string>> repeat_count;
        std::optional<std::string> name;

        std::string get_var_name() const
        {
            if (!name)
            {
                return "";
            }
            else
            {
                return *name;
            }
        }
        void print() const {
            std::cout << "Type: " << type;

            if (repeat_count) {
                std::cout << ", Repeat: ";
                if (std::holds_alternative<int>(*repeat_count)) {
                    std::cout << std::get<int>(*repeat_count);
                }
                else {
                    std::cout << "\"" << std::get<std::string>(*repeat_count) << "\"";
                }
            }

            if (name) {
                std::cout << ", Name: " << *name;
            }

            std::cout << std::endl;
        }
    };


    export class Intepreter
    {
    public:
        Intepreter()
        {
            init_read_handlers();
            init_evaluator();
        }
        ExecR get_error(const std::string& error)
        {
            fmt::print(fmt::fg(fmt::color::red), "{}\n", error);
            set_num_var(ERR, 1);
            return std::unexpected<ExecutionInfo>(error);
        }
        ExecR execute(const std::string& cmd)
        {
            std::smatch matches;
            if (cmd.starts_with("sh ") || cmd == "sh")
            {
                std::string cmd2 = cmd.substr(3);
                //std::cout << "sh: " << cmd2 << std::endl;
                return execute_shell(cmd2);
            }
            else if (std::regex_match(cmd, matches, assignment_patter_)) {
                return execute_assignment(matches[1].str(), matches[2].str());
            }
            else
            {
                auto r = evaluator_.evaluate(cmd);
                if (r)
                {
                    std::cout << *r << std::endl;
                    return ExecutionResult{};
                }
                else
                {
                    return get_error("Failed to evaluate expression.");
                }
            }
            return get_error(std::format("Unrecognized command: {}", cmd));
        }
        ExecR execute_assignment(const std::string& var, const std::string& expr)
        {
            double val = 0;
            auto end = expr.data() + expr.size();
            auto [ptr, cp] = std::from_chars(expr.data(), end, val);
            if (ptr != end)
            {
                return get_error("Not handled.");
            }
            set_num_var(var, val);
            return ExecutionResult{};
        }

        ExecR execute_open_file(const std::string& filename, const std::string& name)
        {
            /* FileHandle file_handle;
             file_handle.name = name;
             file_handle.file_name = filename;*/
            MemoryMapFileHandle file_handle;
            file_handle.file_ptr = std::make_shared<boost::iostreams::mapped_file_source>();
            file_handle.file_ptr->open(filename, std::ios::binary);
            if (!file_handle.file_ptr->is_open())
            {
                return std::unexpected<ExecutionInfo>{"Failed to open file."};
            }
            if (name.size() > 0) {
                symbols_.emplace(name, file_handle);
            }
            symbols_.emplace("CF", file_handle);
            return ExecutionResult{};
        }

        int eval_type_count(const TypeExpression& expr)
        {
            if (!expr.repeat_count)
            {
                return 1;
            }
            if (std::holds_alternative<int>(*expr.repeat_count)) {
                // Step 3: Get the int value
                int number = std::get<int>(*expr.repeat_count);
                std::cout << "The int value is: " << number << std::endl;
                return number;
            }
            else {
                std::cout << "Value is not an int" << std::endl;
                return 0;
            }
        }

        ExecR execute_read_file(const std::vector<std::string>& operands, bool move_pointer = false)
        {
            auto handle = std::get_if<MemoryMapFileHandle>(&symbols_["CF"]);
            if (!handle)
            {
                return std::unexpected<ExecutionInfo>{"No file opened."};
            }
            auto pos = get_pos();
            for (int i = 0; i < operands.size(); ++i) {
                const auto& operand = operands[i];
                auto p = parse_type_expression(operand);
                if (p)
                {
                    auto cnt = eval_type_count(*p);
                    auto varname = p->get_var_name();
                    if (read_handlers_.contains(p->type))
                    {
                        read_handlers_[p->type](handle, pos, cnt, varname);
                    }
                    else if (p->type.starts_with("i"))
                    {
                        auto bl = p->type.substr(1);
                        int bln = 0;
                        try {
                            bln = std::stoi(bl);
                        }
                        catch (const std::exception& ex)
                        {
                            auto msg = std::format("Give ({}), sssuming byte array, but failed to parse length.", bl);
                            std::println(std::cout, "{}", msg);
                            return std::unexpected<ExecutionInfo>{msg};
                        }
                        for (int j = 0; j < cnt; ++j)
                        {
                            pos += bln;
                        }
                    }
                    else if (p->type.starts_with("b"))
                    {
                        auto bl = p->type.substr(1);
                        int bln = 0;
                        try {
                            bln = std::stoi(bl);
                        }
                        catch (const std::exception& ex)
                        {
                            auto msg = std::format("Give ({}), sssuming byte array, but failed to parse length.", bl);
                            std::println(std::cout, "{}", msg);
                            return std::unexpected<ExecutionInfo>{msg};
                        }
                        for (int j = 0; j < cnt; ++j)
                        {
                            pos += bln;
                        }
                    }
                    else
                    {
                        auto msg = std::format("Invalid type: ({}). Supporting format like: i32, i64*2, b2*4", operand);
                        std::println(std::cout, "{}", msg);
                        return std::unexpected<ExecutionInfo>{msg};
                    }
                }
                else
                {
                    auto msg = std::format("Invalid operand: ({}). Supporting format like: i32, i64*2, b2*4", operand);
                    std::println(std::cout, "{}", msg);
                    return std::unexpected<ExecutionInfo>{msg};
                }
            }
            if (move_pointer)
            {
                set_pos(pos);
            }
            return ExecutionResult{};
        }
        std::optional<TypeExpression> parse_type_expression(const std::string& expr) {
            static const std::regex pattern(
                R"(([a-z][a-z0-9]*)(?:\*(?:([0-9]+)|\$\{([^}]*)\}))?(?:\(([a-zA-Z][a-zA-Z0-9]*)\))?$)"
            );

            std::smatch matches;
            if (std::regex_match(expr, matches, pattern)) {
                TypeExpression result;

                result.type = matches[1].str();

                if (matches[2].matched && !matches[2].str().empty()) {
                    try {
                        result.repeat_count = std::stoi(matches[2].str());
                    }
                    catch (...) {
                        return std::nullopt;
                    }
                }
                else if (matches[3].matched && !matches[3].str().empty()) {
                    result.repeat_count = matches[3].str();
                }

                if (matches[4].matched && !matches[4].str().empty()) {
                    result.name = matches[4].str();
                }

                return result;
            }

            return std::nullopt;
        }
        template<typename T>
        void read_handler(MemoryMapFileHandle* handle, int64_t& pos, int sz, int cnt, const std::string& varname)
        {
            if (cnt == 1)
            {
                handle_read_item<T>(handle, pos, sz, varname);
            }
            else
            {
                handle_read_item<T>(handle, pos, sz, cnt, varname);
            }
            pos += sz;
        }
        template<typename T>
        T handle_read_item(MemoryMapFileHandle* handle, int64_t& pos, int sz, const std::string& varname)
        {
            T item = *reinterpret_cast<const T*>(handle->data() + pos);
            std::cout << item << std::endl;
            pos += sz;
            if (!varname.empty())
            {
                set_num_var(varname, item);
            }
            return item;
        }
        template<typename T>
        std::vector<T> handle_read_item(MemoryMapFileHandle* handle, int64_t& pos, int sz, int count, const std::string& varname)
        {
            std::vector<T> items(count);
            for (int i = 0; i < count; ++i)
            {
                items[i] = *reinterpret_cast<const T*>(handle->data() + pos);
                pos += sz;
            }
            std::cout << fmt::format("{}", fmt::join(items, ",")) << std::endl;
            return items;
        }
        ExecR execute_move(const std::string& operand, bool rel = true)
        {
            auto handle = std::get_if<MemoryMapFileHandle>(&symbols_["CF"]);
            if (!handle)
            {
                return std::unexpected<ExecutionInfo>{"No file opened."};
            }

            auto end = operand.data() + operand.size();
            int64_t p = 0;
            auto [ptr, cp] = std::from_chars(operand.data(), end, p);
            if (ptr != end)
            {
                return std::unexpected<ExecutionInfo>{"Invalid operand."};
            }
            /*if (p < 0 || p >= handle->file_ptr->size())
            {
                return std::unexpected<ExecutionInfo>{"Invalid position."};
            }*/

            auto pos = get_pos();
            if (rel) {
                set_pos(pos + p);
            }
            else
            {
                set_pos(p);
            }
            std::println("POS={}", get_pos());
            return ExecutionResult{};
        }
        std::string doubleToString(double value) {
            std::ostringstream oss;
            oss << std::setprecision(15) << value;
            std::string str = oss.str();
            if (str.find('.') != std::string::npos) {
                str = str.substr(0, str.find_last_not_of('0') + 1);
                if (str.back() == '.') {
                    str.pop_back();
                }
            }

            return str;
        }
        std::optional<std::string> pre_process(const std::string& input)
        {
            std::string result = input;
            std::regex pattern("\\$\\{([^}]+)\\}");

            std::smatch match;
            std::string::const_iterator searchStart(input.begin());
            std::string::const_iterator inputEnd(input.end());

            std::vector<std::pair<size_t, std::pair<size_t, std::string>>> replacements;

            bool failed = false;
            while (std::regex_search(searchStart, inputEnd, match, pattern)) {
                size_t position = std::distance(input.begin(), match[0].first);
                size_t length = match[0].length();

                std::string expression = match[1].str();
                auto r = evaluator_.evaluate(expression);
                std::string replacement;
                if (r)
                {
                    replacement = doubleToString(*r);
                }
                else
                {
                    failed = true;
                    break;
                    //replacement = "";
                }
                //= replacementFunc(expression);

                replacements.push_back({ position, {length, replacement} });

                searchStart = match.suffix().first;
            }

            if (failed) return {};

            for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
                size_t position = it->first;
                size_t length = it->second.first;
                const std::string& replacement = it->second.second;

                result.replace(position, length, replacement);
            }

            return result;
        }
        ExecR execute_shell(const std::string& args)
        {
            //for (int )
            auto r = pre_process(args);
            if (r)
            {
                std::cout << *r << std::endl;
            }
            return ExecutionResult{};
        }
        int64_t get_pos()
        {
            auto pos = static_cast<int64_t>(*std::get_if<double>(&symbols_["POS"]));
            return pos;
        }
        double& get_pos_var()
        {
            auto pos = std::get_if<double>(&symbols_["POS"]);
            /* if (!pos)
             {
                 throw std::runtime_error("POS not found");
             }*/
            return *pos;
        }
        void set_pos(const int64_t& new_pos)
        {
            //auto pos = std::get_if<int64_t>(&symbols_["POS"]);
            //symbols_["POS"] = static_cast<double>(new_pos);
            set_num_var(POS, new_pos);
        }

        template<typename T>
        void set_num_var(const std::string& name, const T& v)
        {
            symbols_[name] = static_cast<double>(v);
            evaluator_.add_variable(name, get_num_var(name));
        }
        template<typename T>
        T get_num(const std::string& name)
        {
            return static_cast<T>(get_num_var(name));
        }
        double& get_num_var(const std::string& name)
        {
            auto pos = std::get_if<double>(&symbols_[name]);
            if (!pos)
            {
                throw std::runtime_error("POS not found");
            }
            return *pos;
        }

        int get_last_error()
        {
            return get_num<int>(ERR);
        }
    protected:

        void init_read_handlers()
        {
            read_handlers_.emplace("i32",
                [this](MemoryMapFileHandle* handle, int64_t& pos, int cnt, const std::string& varname) {
                    read_handler<int32_t>(handle, pos, 4, cnt, varname);
                }
            );
            read_handlers_.emplace("i64",
                [this](MemoryMapFileHandle* handle, int64_t& pos, int cnt, const std::string& varname) {
                    read_handler<int64_t>(handle, pos, 8, cnt, varname);
                }
            );
            read_handlers_.emplace("f32",
                [this](MemoryMapFileHandle* handle, int64_t& pos, int cnt, const std::string& varname) {
                    read_handler<float>(handle, pos, 4, cnt, varname);
                }
            );
            read_handlers_.emplace("f64",
                [this](MemoryMapFileHandle* handle, int64_t& pos, int cnt, const std::string& varname) {
                    read_handler<double>(handle, pos, 8, cnt, varname);
                }
            );
        }

        void init_evaluator()
        {
            evaluator_.add_variable(POS, get_num_var(POS));
            evaluator_.add_variable(SZ, get_num_var(SZ));

        }
        std::unordered_map<std::string, Symbol> symbols_{ {"POS",0.0},{"SZ",0.0} ,{"ERR",0.0} };
        std::unordered_map < std::string, std::function<void(MemoryMapFileHandle* handle, int64_t& pos, int cnt, const std::string& varname)>> read_handlers_;

        std::regex byte_pattern_{ "^b\\d+$" };
        std::regex assignment_patter_{ "^([A-Za-z][A-Za-z0-9]*)\\s*=\\s*(.+)$" };

        ExpressionEvaluator evaluator_;

    };
}
