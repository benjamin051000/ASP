
# Keep track of ids
users = {}


# Get input
while (input_ := input()):
    tup = eval(input_)  # Get as tuple
    # Format
    id_, topic, score = tup

    if id_ in users:
        user = users[id_]
        
        if topic in user:
            user[topic] += score
        else:
            user[topic] = score
        
    else:
        users[id_] = {}  # To store topics and scores
    
    

