function BrokenConnect()
{
    emiter.emitted.connect(receive)
}

function receive()
{
    print("function receive()");
    // this should throw an exception, "foo" does not exist
    foo.bar = "test";
}
