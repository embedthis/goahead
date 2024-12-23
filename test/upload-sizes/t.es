let http: Http = new Http

let i = App.args[1] - 0

let buf = 'a'.times(i) + '\n'
Path('a.dat').write(buf)

http.upload('http://127.0.0.1:18080/action/uploadTest', { myfile: 'a.dat' } )
http.wait()

if (http.status != 200) {
    print('Failed', i)
} else {
    print(http.response)
    let data = Path('../web/tmp/a.dat').readString()

    if (data.toString() == buf.toString()) {
        print('Passed', i)
    } else {
        print('Failed')
        print('DATA', data.length) ; print(data)
        print('BUF', buf.length) ; print(buf)
    }
}
http.close()
