import psycopg2

def connect():
    conn = psycopg2.connect(database = "datacamp_courses",
                    user = "datacamp",
                    host= 'localhost',
                    password = "postgresql_tutorial",
                    port = 5432)

    return conn

def query(sql: str, params: tuple):
    try:
        conn = connect()
        cur = conn.cursor()
        cur.execute(sql, tuple(params))
        conn.commit()
        cur.close()
        conn.close()
    except Exception as e:
        print("erreur db vvvvvvvv")
        print(e)
