from flask import Flask, render_template, request
import subprocess
import os
import ctypes

app = Flask(__name__)

# Load compiled shared library
lib = ctypes.CDLL('./libdb.so')

# Set up arguments for insert row functions (char *filename, char **col_vals)
lib.query_insert_row.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p)]
# No return type for insert row
lib.query_insert_row.restype = None

# char **return_all(int n)
lib.return_all.argtypes = [ctypes.POINTER(ctypes.c_int)]
lib.return_all.restype = ctypes.POINTER(ctypes.c_char_p)

# char **parse_query(char **cols, int num_cols, char *filtered_col, int amount)
lib.parse_query.argtypes = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_int), ctypes.c_char_p]
lib.parse_query.restype = ctypes.POINTER(ctypes.c_char_p)

lib.query_create_table.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p), ctypes.c_int]
lib.query_create_table.restype = ctypes.c_int

#char **parse_query(char *filtered_col, int amount){
#lib.parse_query.argtypes = [ctypes.c_char_p, ctypes.c_int]
#lib.parse_query.restype = ctypes.POINTER(ctypes.c_char_p)

# char **project(char **cols, int n)
#lib.project.argtypes = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_int]
#lib.project.restype = ctypes.POINTER(ctypes.c_char_p)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/create', methods=['POST'])
def create():
    schema_name = request.form.get('schema_name')
    c_schema_name = schema_name.encode('utf-8')
    fields = request.form.getlist('field')

    EncodedArray = ctypes.c_char_p * (len(fields) + 1)
    c_fields = EncodedArray(*(f.encode('utf-8') for f in fields), None)
    result = lib.query_create_table(c_schema_name, c_fields, len(fields))

    return f"<p>{'Created' if result else 'Failed'}</p><a href='/'>Go Back</a>"


@app.route('/insert', methods=['POST'])
def insert():
    filename = request.form['filename']
    values = request.form.getlist('value')

    EncodedArray = ctypes.c_char_p * (len(values) + 1)
    c_values = EncodedArray(*(v.encode('utf-8') for v in values), None)


    lib.query_insert_row(filename.encode('utf-8'), c_values)

    return "<p>Row inserted successfully.</p><a href='/'>Back</a>"


@app.route('/retrieve', methods=['GET'])
def retrieve_all():
    #row_amount = ctypes.c_int()
    #rows = lib.return_all(ctypes.byref(row_amount))

    #if not rows or row_amount.value == 0:
    #    return render_template('retrieve.html')

    #results = [rows[i].decode('utf-8') for i in range(row_amount.value)]
    return render_template('retrieve.html', results=None)

# Expected url for query SELECT state, date FROM schema WHERE state = 1;
# should look like: /query?state=1&cols=state&cols=date
# For a full table retrieval url should have cols=*
# char **parse_query(char **cols, int num_cols, char *filtered_col, int amount)
@app.route('/query', methods=['POST'])
def handle_query():
    print("got", request.form, request)
    selected_cols = request.form.getlist('cols')
    selected_field = request.form['field']
    selected_val = int(request.form['value'])
    selected_op = request.form['operator']

    print("got", selected_cols, selected_field, selected_val, selected_op)
    final_amount = ctypes.c_int()
    EncodedArray = ctypes.c_char_p * (len(selected_cols) + 1)
    c_cols = EncodedArray(*(col.encode('utf-8') for col in selected_cols), None)
    c_cols_len = ctypes.c_int(len(selected_cols))

    c_field = ctypes.c_char_p(selected_field.encode('utf-8'))
    c_op = ctypes.c_char_p(selected_op.encode('utf-8'))
    c_val = ctypes.c_int(selected_val)

    rows = lib.parse_query(c_cols, c_cols_len, c_field, c_val, ctypes.byref(final_amount), c_op)
    
    # Walk until NULL
    results = []
    i = 0
    while i < final_amount.value:
        results.append(rows[i].decode("utf-8"))
        i += 1

    print("Got results:", results)
    return render_template('retrieve.html', results=results)
  

    
#@app.route('/project', methods=['POST'])
#def project():
    #selected_cols = request.form.getlist('value')
    #col_nums = ctypes.c_int(len(selected_cols))
    #print("got", selected_cols, "from", request.form)
    #EncodedArray = ctypes.c_char_p * (len(selected_cols) + 1)
    #result = EncodedArray(*(col.encode('utf-8') for col in selected_cols), None)

    #lib.project(result, col_nums)


#    selected_col = request.form['field']
#    selected_val = int(request.form['value'])
    

#    c_col = ctypes.c_char_p(selected_col.encode('utf-8'))
#    c_val = ctypes.c_int(selected_val)
#    lib.parse_query(c_col, c_val)
#    return "<p>Testing only, you should not be able to see this</p>"


if __name__ == '__main__':
    app.run(debug=True)