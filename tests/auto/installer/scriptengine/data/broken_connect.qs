function BrokenConnect()
{
    emiter.emitted.connect(receive)
}

function receive()
{
    print("function receive()");
    // this will print an error.
    foo.bar = "test";
}
