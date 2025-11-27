#include "Common.hpp"
#include "Parser.hpp"

enum SeverityLevel
{
    Note,
    Hint,
    Info,
    Warning,
    SyntaxError,
    Error,
    Fatal,
};

class CompileError
{
public:
    SeverityLevel Severity;
    std::string Message;
    ScriptLocation Location;

    CompileError()
    {
    }

    CompileError(std::string message, SeverityLevel severity, ScriptLocation location = ScriptLocation())
        : Severity(severity), Message(message), Location(location) {}

    std::string ToString(const bool Raw = false, const bool ShowFile = true, const bool ShowLocation = true)
    {
        std::string NewMessage = Message;

        // Check if string starts with a pipe
        if (!Raw && !NewMessage.empty() && NewMessage[0] == '|')
        {
            // Find the second pipe
            size_t secondPipe = NewMessage.find('|', 1);

            if (secondPipe != std::string::npos)
            {
                // Determine start of remaining text
                size_t startPos = secondPipe + 1;

                // If there’s a space right after the second '|', skip it too
                if (startPos < NewMessage.size() && NewMessage[startPos] == ' ')
                {
                    ++startPos;
                }

                // Remove everything before startPos
                NewMessage.erase(0, startPos);
            }
        }

        if (ShowLocation)
            return Location.ToString(ShowFile) + " [\x1b[93m" + std::string(magic_enum::enum_name(Severity)) + "\x1b[0m]: \x1b[1;31m" + NewMessage + "\x1b[0m";
        else
            return "[\x1b[93m" + std::string(magic_enum::enum_name(Severity)) + "\x1b[0m]: \x1b[1;31m" + NewMessage + "\x1b[0m";
    }
};

