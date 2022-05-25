/*
TEST_OUTPUT:
---
fail_compilation/ice9284.d(14): Error: none of the overloads of template `ice9284.C.__ctor` are callable using argument types `!()(int)`
fail_compilation/ice9284.d(12):        Candidate is: `__ctor()(string)`
fail_compilation/ice9284.d(20): Error: template instance `ice9284.C.__ctor!()` error instantiating
---
*/

class C
{
    this()(string)
    {
        this(10);
        // delegating to a constructor which not exists.
    }
}
void main()
{
    new C("hello");
}
