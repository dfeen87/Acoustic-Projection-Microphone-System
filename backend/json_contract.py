"""Decode external JSON evidence without ambiguous keys or numeric extensions."""
import json
import math


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def _invalid_constant(value):
    raise ValueError(f"Invalid JSON numeric constant: {value}")


def _finite_float(value):
    result = float(value)
    if not math.isfinite(result):
        raise ValueError("JSON numeric value is outside the finite domain")
    return result


def loads(raw):
    try:
        result = json.loads(raw, object_pairs_hook=_unique_object, parse_constant=_invalid_constant,
                            parse_float=_finite_float)
    except RecursionError as exc:
        raise ValueError("JSON nesting exceeds decoder limits") from exc
    # json.loads accepts lone escaped UTF-16 surrogates, which cannot be UTF-8
    # protocol text. Check keys and nested values before trusting the object.
    pending = [result]
    while pending:
        value = pending.pop()
        if isinstance(value, str):
            value.encode("utf-8")
        elif isinstance(value, dict):
            pending.extend(value.keys())
            pending.extend(value.values())
        elif isinstance(value, list):
            pending.extend(value)
    return result
