/*!
 * @file CNonCopyable.h
 * @brief Non-copyable base class to prevent object copying and assignment
 * @details Inherit from this class to automatically prevent copying and assignment
 */
#pragma once

#ifdef _MSC_VER
#pragma warning(disable : 4275)
#endif

namespace CVC
{
    class CNonCopyable
    {
    protected:
        CNonCopyable(void) {}
        ~CNonCopyable(void) {}

    private:
        CNonCopyable(const CNonCopyable &);
        CNonCopyable &operator=(const CNonCopyable &);
    };

}
