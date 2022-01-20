import re

with open('input.txt') as f:
    string = f.read()

tups = re.findall(r"\(\s*(\S+)\s*,\s*(\S+)\s*,\s*(\S+)\s*\)", string)

actions = {
    'P': 50,  # Post
    'L': 20,  # Like
    'D': -10, # Dislike
    'C': 30,  # Comment
    'S': 40,  # Share
}

for id_, action, topic in tups:
    print((int(id_), topic, actions[action]))
