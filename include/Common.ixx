module;
// #include <boost/iostreams/device/mapped_file.hpp>

export module LibIBP.Common;

import std;
import <boost/iostreams/device/mapped_file.hpp>;

export namespace IBP {
    enum class OP {
        OPEN,
        READ,
        MOVE,
        SET
    };

    enum class SymbolType
    {
        FILE_HANDLE,
    };
    enum class VarType
    {
        I32, I64, F32, F64,
    };

    struct FileHandle
    {
        std::string name;

        std::string file_name;
        size_t file_size;

        std::ifstream file_stream;
    };
    struct MemoryMapFileHandle
    {
        std::shared_ptr<boost::iostreams::mapped_file_source> file_ptr;
        const auto data() const
        {
            return file_ptr->data();
        }
    };
    struct NumVar
    {
        VarType type;
        double value;
    };
    using Symbol = std::variant<MemoryMapFileHandle, NumVar>;
    /*using ReadResult = std::variant<int32_t, int64_t, float, double, std::vector<int32_t>, std::vector<>>;*/

    struct ExecutionResult
    {

    };
    struct ExecutionInfo
    {
        std::string info;
    };
    using ExecR = std::expected<ExecutionResult, ExecutionInfo>;

    const std::string POS{ "POS" };
    const std::string SZ{ "SZ" };
    const std::string ERR{ "ERR" };
}