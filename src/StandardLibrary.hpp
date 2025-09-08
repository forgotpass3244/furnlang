#pragma once
#include "Interpreter.hpp"
#include "Common.hpp"

const char *IO = std::getenv("TERM_PROGRAM");
const bool IsIntegratedTerminal = IO != nullptr && std::string(IO) == "vscode";

Value CreateStdLib()
{
    exfunc Exit def
    {
        std::exit(std::any_cast<rt_Int>(Args[0]));
    };

    exfunc Length def
    {
        rt_Int len = 0;
        forarg
        {
            if (Arg.type() == typeid(std::string))
                len += std::any_cast<std::string>(Arg).length();
            else if (Arg.type() == typeid(MapReference))
                len += std::any_cast<MapReference>(Arg).Length();
        }

        return len;
    };

    exfunc Dispose def
    {
        forarg
        {
            if (Arg.type() == typeid(MapReference))
                std::any_cast<MapReference>(Arg).Unstore();
            else if (Arg.type() == typeid(ClassObject))
                std::any_cast<ClassObject>(Arg).Members->Unstore();
    else throw std::runtime_error("Unable to dispose of type '" + std::string(magic_enum::enum_name(MakeTypeDescriptor(AnyToValue(Arg)).Type)) + "', expected map or class object");
        }

        ret
    };

    exfunc Clone def
    {
        std::any Arg = Args[0];
        if (Arg.type() == typeid(MapReference))
            return std::any_cast<MapReference>(Arg).Clone();
        else if (Arg.type() == typeid(ClassObject))
        {
            ClassObject Object = std::any_cast<ClassObject>(Arg);
            Object.Members = std::make_shared<MapReference>(Object.Members->Clone());
            return Object;
        }
        else throw std::runtime_error("Unable to clone object type '" + std::string(magic_enum::enum_name(MakeTypeDescriptor(AnyToValue(Arg)).Type)) + "', expected map or class object");
    };

    exfunc Dynamic def
    {
        Any Arg = std::any_cast<Any>(Args[0]);
        return Arg.Val;
    };

    exfunc IntToChar def
    {
        std::string Result;
        forarg
        {
            if (Arg.type() == typeid(rt_Int))
                Result += static_cast<char>(std::any_cast<rt_Int>(Arg));
            else if (Arg.type() == typeid(rt_Float))
                Result += static_cast<char>(static_cast<rt_Int>(std::any_cast<rt_Float>(Arg)));
            else
                return nullptr;
        }

        return Result;
    };

    Library StdLib;

    StdLib["Exit"] = Exit;
    StdLib["Length"] = Length;
    StdLib["Dispose"] = Dispose;
    StdLib["Clone"] = Clone;
    StdLib["Dynamic"] = Dynamic;
    StdLib["IntToChar"] = IntToChar;

    exfunc WriteLn def
    {
        setvbuf(stdout, nullptr, _IOFBF, 65536); // large 64 KB buffer
        forarg
            std::cout << ToString(Arg);
        std::cout << '\n';
        ret
    };

    exfunc Write def
    {
        forarg
            std::cout << ToString(Arg);
        ret
    };

    exfunc ReadLn def
    {
        std::string Input;
        std::getline(std::cin, Input);
        return Input;
    };

    exfunc FlushIO def
    {
        std::cout << std::flush;
        ret
    };

    exfunc ClearIO def
    {
        std::cout << std::flush;

#if defined _WIN32
            system("cls");
        #elif defined(__LINUX__) || defined(__gnu_linux__) || defined(__linux__)
            system("clear");
        #elif defined(__APPLE__)
            system("clear");
        #endif

        if (IsIntegratedTerminal)
        {
            std::cout << u8"\033[2J\033[1;1H";
            std::cout.flush();
        }

        ret
    };

    Library Console;
    Console["WriteLn"] = WriteLn;
    Console["Write"] = Write;
    Console["ReadLn"] = ReadLn;
    Console["Flush"] = FlushIO;
    Console["TryClearIO"] = ClearIO;
    StdLib["Console"] = Console;

    exfunc SystemExecute def
    {
        forarg
            std::system(ToString(Arg).c_str());
        ret
    };

    exfunc Sleep def
    {
        std::this_thread::sleep_for(std::chrono::duration<rt_Float>(ToFloat(Args[0])));
        ret
    };

    exfunc Wait def
    {
        rt_Float Seconds = ToFloat(Args[0]);
        rt_Int Accuracy = ToInt(Args[1]);

        auto StartTime = std::chrono::high_resolution_clock::now();

        while (true)
        {
            auto Now = std::chrono::high_resolution_clock::now();
            rt_Float Elapsed = std::chrono::duration<rt_Float>(Now - StartTime).count();

            if (Elapsed >= Seconds)
                break;

            // Sleep quickly so it doesnt use all the CPU (though it will be less accurate)
            std::this_thread::sleep_for(std::chrono::milliseconds(Accuracy));
        }

        ret
    };

    exfunc TimeSinceYear def
    {
        // Set up Jan 1, 2025 at 00:00:00
        std::tm TmStart = {};
        TmStart.tm_year = ToInt(Args[0]) - 1900; // Years since 1900
        TmStart.tm_mon = 0;                                                // January
        TmStart.tm_mday = 1;                                               // Day 1

        // Convert to time_t
        std::time_t TimeStart = std::mktime(&TmStart);

        // Convert to chrono::system_clock::time_point
        std::chrono::system_clock::time_point jan1_yr =
            std::chrono::system_clock::from_time_t(TimeStart);

        // Get current time
        std::chrono::system_clock::time_point Now = std::chrono::system_clock::now();

        // Compute duration in seconds as rt_Float
        const rt_Float Seconds = std::chrono::duration_cast<std::chrono::duration<rt_Float>>(Now - jan1_yr).count();

        return Seconds;
    };

    Library System;
    System["Execute"] = SystemExecute;
    System["Sleep"] = Sleep;
    System["Wait"] = Wait;
    System["TimeSinceYear"] = TimeSinceYear;
    StdLib["System"] = System;

    return Value(StdLib);
}
