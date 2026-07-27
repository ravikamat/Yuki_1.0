$prompts = @(
    "What is your name?",
    "My name is Ravi.",
    "What is my name?",
    "Alpha means beginning.",
    "What does alpha mean?",
    "My name is John.",
    "What would happen if the sun vanished?",
    "Why does rain fall?",
    "How is an atom like a solar system?",
    "Tell me a haiku about winter.",
    "Create a creature by blending an eagle and a shark.",
    "What are you thinking right now?"
)

$inputString = ($prompts -join "`n") + "`nexit`n"
$inputString | d:\Yuki_1.0\build\Release\yuki.exe
