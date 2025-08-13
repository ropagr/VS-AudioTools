// SPDX-License-Identifier: MIT

#include <format>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "VapourSynth4.h"

namespace vsmap
{
    static std::string initVarNotFoundErrorMsg(const char* varName, const char* logFuncName)
    {
        return std::format("{}: could not find: {}", logFuncName, varName);
    }


    VSNode* getOptClip(const char* varName, const VSMap* in, const VSAPI* vsapi, VSNode* defaultValue)
    {
        int err = 0;
        VSNode* result = vsapi->mapGetNode(in, varName, 0, &err);
        if (err)
        {
            return defaultValue;
        }

        return result;
    }


    bool getOptBool(const char* varName, const VSMap* in, const VSAPI* vsapi, bool defaultValue)
    {
        int err = 0;
        int64_t result = vsapi->mapGetInt(in, varName, 0, &err);
        if (err)
        {
            return defaultValue;
        }

        return static_cast<bool>(result);
    }


    double getOptDouble(const char* varName, const VSMap* in, const VSAPI* vsapi, double defaultValue)
    {
        int err = 0;
        double result = vsapi->mapGetFloat(in, varName, 0, &err);
        if (err)
        {
            return defaultValue;
        }

        return result;
    }


    float getOptFloat(const char* varName, const VSMap* in, const VSAPI* vsapi, float defaultValue)
    {
        int err = 0;
        float result = vsapi->mapGetFloatSaturated(in, varName, 0, &err);
        if (err)
        {
            return defaultValue;
        }

        return result;
    }


    int getOptInt(const char* varName, const VSMap* in, const VSAPI* vsapi, int defaultValue)
    {
        int err = 0;
        int result = vsapi->mapGetIntSaturated(in, varName, 0, &err);
        if (err)
        {
            return defaultValue;
        }

        return result;
    }


    int64_t getOptInt64(const char* varName, const VSMap* in, const VSAPI* vsapi, int64_t defaultValue)
    {
        int err = 0;
        int64_t result = vsapi->mapGetInt(in, varName, 0, &err);
        if (err)
        {
            return defaultValue;
        }

        return result;
    }


    std::optional<std::vector<int>> getIntArray(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi)
    {
        int err = 0;
        // detect if an array was provided by the user
        vsapi->mapGetIntArray(in, varName, &err);
        if (err)
        {
            vsapi->mapSetError(out, initVarNotFoundErrorMsg(varName, logFuncName).c_str());
            return std::nullopt;
        }

        std::vector<int> result;
        int arrayLen = vsapi->mapNumElements(in, varName);
        if (0 < arrayLen)
        {
            result.reserve(static_cast<size_t>(arrayLen));
        }

        for (int i = 0; i < arrayLen; ++i)
        {
            int value = vsapi->mapGetIntSaturated(in, varName, i, &err);
            if (err)
            {
                // not supposed to happen
                vsapi->mapSetError(out, initVarNotFoundErrorMsg(varName, logFuncName).c_str());
                return std::nullopt;
            }

            result.push_back(value);
        }

        return result;
    }


    std::vector<int> getOptIntArray(const char* varName, const VSMap* in, const VSAPI* vsapi, const std::vector<int>& defaultValue)
    {
        std::vector<int> result;

        // detect if an array was provided by the user
        int err = 0;
        vsapi->mapGetIntArray(in, varName, &err);
        if (err)
        {
            result.assign(defaultValue.begin(), defaultValue.end());
            //result.insert(result.end(), defaultValue.begin(), defaultValue.end());
            /*
            for (size_t i = 0; i < defaultValue.size(); ++i)
            {
                result.push_back(defaultValue[i]);
            }
            */
            return result;
        }

        int arrayLen = vsapi->mapNumElements(in, varName);

        if (0 < arrayLen)
        {
            result.reserve(static_cast<size_t>(arrayLen));

            for (int i = 0; i < arrayLen; ++i)
            {
                int value = vsapi->mapGetIntSaturated(in, varName, i, &err);
                if (err)
                {
                    // not supposed to happen -> ignore
                    err = 0;
                }
                else
                {
                    result.push_back(value);
                }
            }
        }
        return result;
    }


    std::optional<std::vector<int64_t>> getInt64Array(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi)
    {
        int err = 0;
        // detect if an array was provided by the user
        vsapi->mapGetIntArray(in, varName, &err);
        if (err)
        {
            vsapi->mapSetError(out, initVarNotFoundErrorMsg(varName, logFuncName).c_str());
            return std::nullopt;
        }

        std::vector<int64_t> result;
        int arrayLen = vsapi->mapNumElements(in, varName);
        if (0 < arrayLen)
        {
            result.reserve(static_cast<size_t>(arrayLen));

            for (int i = 0; i < arrayLen; ++i)
            {
                int64_t value = vsapi->mapGetInt(in, varName, i, &err);
                if (err)
                {
                    // not supposed to happen
                    vsapi->mapSetError(out, initVarNotFoundErrorMsg(varName, logFuncName).c_str());
                    return std::nullopt;
                }

                result.push_back(value);
            }
        }

        return result;
    }


    std::vector<int64_t> getOptInt64Array(const char* varName, const VSMap* in, const VSAPI* vsapi, const std::vector<int64_t>& defaultValue)
    {
        std::vector<int64_t> result;

        // detect if an array was provided by the user
        int err = 0;
        vsapi->mapGetIntArray(in, varName, &err);
        if (err)
        {
            result.assign(defaultValue.begin(), defaultValue.end());
            return result;
        }

        int arrayLen = vsapi->mapNumElements(in, varName);
        if (0 < arrayLen)
        {
            result.reserve(static_cast<size_t>(arrayLen));

            for (int i = 0; i < arrayLen; ++i)
            {
                int64_t value = vsapi->mapGetInt(in, varName, i, &err);
                if (err)
                {
                    // not supposed to happen -> ignore
                    err = 0;
                }
                else
                {
                    result.push_back(value);
                }
            }
        }

        return result;
    }


    std::optional<std::vector<double>> getDoubleArray(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi)
    {
        int err = 0;
        // detect if an array was provided by the user
        vsapi->mapGetFloatArray(in, varName, &err);
        if (err)
        {
            vsapi->mapSetError(out, initVarNotFoundErrorMsg(varName, logFuncName).c_str());
            return std::nullopt;
        }

        std::vector<double> result;
        int arrayLen = vsapi->mapNumElements(in, varName);
        if (0 < arrayLen)
        {
            result.reserve(static_cast<size_t>(arrayLen));

            for (int i = 0; i < arrayLen; ++i)
            {
                double value = vsapi->mapGetFloat(in, varName, i, &err);
                if (err)
                {
                    // not supposed to happen
                    vsapi->mapSetError(out, initVarNotFoundErrorMsg(varName, logFuncName).c_str());
                    return std::nullopt;
                }

                result.push_back(value);
            }
        }

        return result;
    }


    std::vector<double> getOptDoubleArray(const char* varName, const VSMap* in, const VSAPI* vsapi, const std::vector<double>& defaultValue)
    {
        std::vector<double> result;

        // detect if an array was provided by the user
        int err = 0;
        vsapi->mapGetFloatArray(in, varName, &err);
        if (err)
        {
            result.assign(defaultValue.begin(), defaultValue.end());
            return result;
        }

        int arrayLen = vsapi->mapNumElements(in, varName);
        if (0 < arrayLen)
        {
            result.reserve(static_cast<size_t>(arrayLen));

            for (int i = 0; i < arrayLen; ++i)
            {
                double value = vsapi->mapGetFloat(in, varName, i, &err);
                if (err)
                {
                    // not supposed to happen -> ignore
                    err = 0;
                }
                else
                {
                    result.push_back(value);
                }
            }
        }

        return result;
    }


    std::optional<std::vector<float>> getFloatArray(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi)
    {
        int err = 0;
        // detect if an array was provided by the user
        vsapi->mapGetFloatArray(in, varName, &err);
        if (err)
        {
            vsapi->mapSetError(out, initVarNotFoundErrorMsg(varName, logFuncName).c_str());
            return std::nullopt;
        }

        std::vector<float> result;
        int arrayLen = vsapi->mapNumElements(in, varName);
        if (0 < arrayLen)
        {
            result.reserve(static_cast<size_t>(arrayLen));

            for (int i = 0; i < arrayLen; ++i)
            {
                float value = vsapi->mapGetFloatSaturated(in, varName, i, &err);
                if (err)
                {
                    // not supposed to happen
                    vsapi->mapSetError(out, initVarNotFoundErrorMsg(varName, logFuncName).c_str());
                    return std::nullopt;
                }

                result.push_back(value);
            }
        }

        return result;
    }


    std::vector<float> getOptFloatArray(const char* varName, const VSMap* in, const VSAPI* vsapi, const std::vector<float>& defaultValue)
    {
        std::vector<float> result;

        // detect if an array was provided by the user
        int err = 0;
        vsapi->mapGetFloatArray(in, varName, &err);
        if (err)
        {
            result.assign(defaultValue.begin(), defaultValue.end());
            return result;
        }

        int arrayLen = vsapi->mapNumElements(in, varName);
        if (0 < arrayLen)
        {
            result.reserve(static_cast<size_t>(arrayLen));

            for (int i = 0; i < arrayLen; ++i)
            {
                float value = vsapi->mapGetFloatSaturated(in, varName, i, &err);
                if (err)
                {
                    // not supposed to happen -> ignore
                    err = 0;
                }
                else
                {
                    result.push_back(value);
                }
            }
        }

        return result;
    }
}
