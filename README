# Columnstore Database


## Overview
This project implements a column-store database system in C. It is designed to handle projection (SELECT), and selection (WHERE) queries. The database uses block-based storage, with records stored in a columnar format. Creation of tables are done using the provided functions in the different files. See the main loop in columnstore.c or example below.


## Features
- **Schema Definition**: Define tables with multiple fields of varying sizes.
- **Columnar Storage**: Data is stored column by column to improve cache performance for projections.
- **Query Support**:
  - **Projection**: Select specific columns or all columns (`SELECT * FROM t1`).
  - **Selection**: Filter records based on a condition (`WHERE age < 10`).
- **Dynamic Table Generation**: Tables and blocks are initialized dynamically.
- **Block Management**: Pages (blocks) are allocated and managed through a custom paging system.


## How It Works
### Schema Definition
- A schema is created using `schema_init()`.
- Fields are added with `add_field()`, each defining a column in the table.


### Inserting Records
- Records are inserted directly into blocks.
- Each block stores values for a single column.


### Query Execution
- **Projection**: Fetches specific columns or all columns from a table.
- **Selection**: Filters rows based on conditions, iterating over blocks containing the relevant column.


## Example
### Initialization and Table Creation
```c
schema_t *sch = schema_init("t1");
add_field(sch, fd_init("age", sizeof(int)));
add_field(sch, fd_init("SSN", sizeof(int)));
add_field(sch, fd_init("numsiblings", sizeof(int)));
generate_table(sch);
```


### Query Execution
```sql
SELECT * FROM t1;
SELECT age, SSN FROM t1;
SELECT age FROM t1 WHERE age < 10;
```


## Compilation and Execution
### Makefile
A simple Makefile is provided to build the project:


### Building and Running
```bash
make
./db
```


## Usage
- Run the program and input SQL-like queries at the prompt.
- NOTE: The syntax is very picky and will segfault if written other than below.
- Example:
```bash
$ SELECT * FROM t1
$ SELECT age, SSN FROM t1 WHERE age < 10
```


## Future Improvements
- **Indexing**: Implement indexing to speed up selection queries.
- **Multiple datatypes**: Allowing for other datatypes other than ints
- **JOIN Support**: Enable queries involving multiple tables.
- **Persistence**: Implement persistent storage to save tables to disk.


## Contributing
Contributions and suggestions are welcome! Feel free to fork the repository and submit pull requests.


## License
MIT License

