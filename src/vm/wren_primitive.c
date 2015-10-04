#include "wren_primitive.h"

#include <math.h>

// Validates that [value] is an integer within `[0, count)`. Also allows
// negative indices which map backwards from the end. Returns the valid positive
// index value. If invalid, reports an error and returns `UINT32_MAX`.
static uint32_t validateIndexValue(WrenVM* vm, uint32_t count, double value,
                                   const char* argName)
{
  if (!validateIntValue(vm, value, argName)) return UINT32_MAX;
  
  // Negative indices count from the end.
  if (value < 0) value = count + value;
  
  // Check bounds.
  if (value >= 0 && value < count) return (uint32_t)value;
  
  vm->fiber->error = wrenStringFormat(vm, "$ out of bounds.", argName);
  return UINT32_MAX;
}

bool validateFn(WrenVM* vm, Value arg, const char* argName)
{
  if (IS_FN(arg) || IS_CLOSURE(arg)) return true;

  vm->fiber->error = wrenStringFormat(vm, "$ must be a function.", argName);
  return false;
}

bool validateNum(WrenVM* vm, Value arg, const char* argName)
{
  if (IS_NUM(arg)) return true;

  vm->fiber->error = wrenStringFormat(vm, "$ must be a number.", argName);
  return false;
}

bool validateIntValue(WrenVM* vm, double value, const char* argName)
{
  if (trunc(value) == value) return true;

  vm->fiber->error = wrenStringFormat(vm, "$ must be an integer.", argName);
  return false;
}

bool validateInt(WrenVM* vm, Value arg, const char* argName)
{
  // Make sure it's a number first.
  if (!validateNum(vm, arg, argName)) return false;

  return validateIntValue(vm, AS_NUM(arg), argName);
}

bool validateKey(WrenVM* vm, Value arg)
{
  if (IS_BOOL(arg) || IS_CLASS(arg) || IS_FIBER(arg) || IS_NULL(arg) ||
      IS_NUM(arg) || IS_RANGE(arg) || IS_STRING(arg))
  {
    return true;
  }

  vm->fiber->error = CONST_STRING(vm, "Key must be a value type or fiber.");
  return false;
}

uint32_t validateIndex(WrenVM* vm, Value arg, uint32_t count,
                       const char* argName)
{
  if (!validateNum(vm, arg, argName)) return UINT32_MAX;

  return validateIndexValue(vm, count, AS_NUM(arg), argName);
}

bool validateString(WrenVM* vm, Value arg, const char* argName)
{
  if (IS_STRING(arg)) return true;

  vm->fiber->error = wrenStringFormat(vm, "$ must be a string.", argName);
  return false;
}

uint32_t calculateRange(WrenVM* vm, ObjRange* range, uint32_t* length,
                        int* step)
{
  *step = 0;

  // Corner case: an empty range at zero is allowed on an empty sequence.
  // This way, list[0..-1] and list[0...list.count] can be used to copy a list
  // even when empty.
  if (*length == 0 && range->from == 0 &&
      range->to == (range->isInclusive ? -1 : 0)) {
    return 0;
  }

  uint32_t from = validateIndexValue(vm, *length, range->from, "Range start");
  if (from == UINT32_MAX) return UINT32_MAX;

  // Bounds check the end manually to handle exclusive ranges.
  double value = range->to;
  if (!validateIntValue(vm, value, "Range end")) return UINT32_MAX;

  // Negative indices count from the end.
  if (value < 0) value = *length + value;

  // Convert the exclusive range to an inclusive one.
  if (!range->isInclusive)
  {
    // An exclusive range with the same start and end points is empty.
    if (value == from)
    {
      *length = 0;
      return from;
    }

    // Shift the endpoint to make it inclusive, handling both increasing and
    // decreasing ranges.
    value += value >= from ? -1 : 1;
  }

  // Check bounds.
  if (value < 0 || value >= *length)
  {
    vm->fiber->error = CONST_STRING(vm, "Range end out of bounds.");
    return UINT32_MAX;
  }

  uint32_t to = (uint32_t)value;
  *length = abs((int)(from - to)) + 1;
  *step = from < to ? 1 : -1;
  return from;
}
