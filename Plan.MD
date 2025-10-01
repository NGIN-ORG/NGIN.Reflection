# Potential Moves to NGIN.Base

- ✅ Reflection now consumes `NGIN::Utilities::Any<>` directly (wrapper removed), so consumers share the same SBO implementation provided by NGIN.Base.
- ✅ The registry uses `NGIN::Utilities::StringInterner<>` from NGIN.Base instead of a bespoke internal copy.
- `include/NGIN/Reflection/NameUtils.hpp`: `MemberNameFromPretty` extracts member names from compiler pretty-function strings. The helper is independent of the registry and could live with other meta utilities in NGIN.Base so that non-reflection code needing consistent member naming can share it.
- `include/NGIN/Reflection/Adapters.hpp`: The container/optional/variant adapters mostly wrap standard containers and NGIN containers into `Any`. With the boxing type now supplied by NGIN.Base, revisiting whether these adapters should live alongside other shared container traits in Base is still on the table.
