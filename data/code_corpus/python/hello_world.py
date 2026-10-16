# hello_world.py
# Teaches: print(), comments, the __main__ guard, and how a Python script runs.
# Read top-to-bottom; every block is annotated. Beginner pedagogical sample.
# Run: python3 hello_world.py


# A function is a named block of code. `def` introduces it.
# `name` is a parameter; the caller chooses what to pass.
def greet(name):
    # An f-string interpolates `{name}` into the surrounding text.
    # Returning a string lets the caller decide what to do with it
    # (print, log, store, etc.).
    return f"Hello, {name}!"


# `main()` is just a convention — Python doesn't require it. We use it
# so the entry point is obvious and easy to find.
def main():
    # `print` writes to standard output, followed by a newline by default.
    print("Hello, world!")

    # Variables don't need a type declaration. The right-hand value
    # determines the type at the moment of assignment.
    visitor = "Athena"

    # Call our function and print whatever it returns.
    message = greet(visitor)
    print(message)


# This idiom guards `main()` so it runs only when the file is executed
# directly (e.g. `python3 hello_world.py`). When another module does
# `import hello_world`, the guard prevents `main()` from firing as a
# side effect of import.
if __name__ == "__main__":
    main()
