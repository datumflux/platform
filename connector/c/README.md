## C/C++과 STAGE:플랫폼의 통신

  > #### source로 첨부된 bson을 사용 하는 방법
  mongodb에서 사용되었던 c용 bson입니다. 현재, 배포중인
bson에 비해 가벼워서 손쉽게 사용하실 수 있습니다.

  1. 추가
     1. bson_init() -> bson_destroy()
     2. bson_finish() 이후 전송 가능
     > bson_size()는 bson_finish() 이후 정상 크기 반환

     3. bson 전송은 bson_data()와 bson_size() 이용해 버퍼와, 크기를 확인 가능

  2. 읽기
     > 순차적으로 접근하는 방식

     1. bson_iterator_from_buffer() 또는 bson_iterator_init()
     2. 다음 데이터로 이동 - bson_iterator_next()
     3. 자료 확인
        1. 자료형 - bson_iterator_type()
        2. 키 - bson_iterator_key()
        3. 값
            - **BSON_STRING**: bson_iterator_string()
            - **BSON_INT**: bson_iterator_int()
            - **BSON_LONG**: bson_iterator_long()
            - **BSON_DOUBLE**: bson_iterator_double()
            - **BSON_OBJECT, BSON_ARRAY**: BSONbson_iterator_subiterator()

     4. 필요한 데이터를 찾는 bson_iterator_find()

        ```c
        bson_type bson_iterator_find( bson_iterator *it, const char *name ) {
            while( bson_iterator_next( it ) ) 
            {
                if ( strcmp( name, bson_iterator_key( it ) ) == 0 )
                    break;
            }
            return bson_iterator_type( it );
        }

        bson_iterator bson_it; bson_iterator_from_buffer(&bson_it, buffer);
        {
            bson_iterator find_it = bson_it;

            if (bson_iterator_find(&find_it, "key") != BSON_EOO)
            {
                ...
            }
        }
        ```

  3. 예제

  ```c
    bson b;
    bson_iterator it;

    /* Create a rich document like this one:
     *
     * { _id: ObjectId("4d95ea712b752328eb2fc2cc"),
     *   user_id: ObjectId("4d95ea712b752328eb2fc2cd"),
     *
     *   items: [
     *     { sku: "col-123",
     *       name: "John Coltrane: Impressions",
     *       price: 1099,
     *     },
     *
     *     { sku: "young-456",
     *       name: "Larry Young: Unity",
     *       price: 1199
     *     }
     *   ],
     *
     *   address: {
     *     street: "59 18th St.",
     *     zip: 10010
     *   },
     *
     *   total: 2298
     * }
     */
    bson_init( &b );
    bson_append_new_oid( &b, "_id" );
    bson_append_new_oid( &b, "user_id" );

    bson_append_start_array( &b, "items" );
    bson_append_start_object( &b, "0" );
    bson_append_string( &b, "name", "John Coltrane: Impressions" );
    bson_append_int( &b, "price", 1099 );
    bson_append_finish_object( &b );

    bson_append_start_object( &b, "1" );
    bson_append_string( &b, "name", "Larry Young: Unity" );
    bson_append_int( &b, "price", 1199 );
    bson_append_finish_object( &b );
    bson_append_finish_object( &b );

    bson_append_start_object( &b, "address" );
    bson_append_string( &b, "street", "59 18th St." );
    bson_append_int( &b, "zip", 10010 );
    bson_append_finish_object( &b );

    bson_append_int( &b, "total", 2298 );

    /* Finish the BSON obj. */
    bson_finish( &b );
    printf("Here's the whole BSON object:\n");
    bson_print( &b );

    /* Advance to the 'items' array */
    bson_find( &it, &b, "items" );

    /* Get the subobject representing items */
    bson_iterator_subobject( &it, &sub );

    /* Now iterate that object */
    printf("And here's the inner sub-object by itself.\n");
    bson_print( &sub );  
  ```

### 참고
> #### 1. BSON     
   - C
     1. https://github.com/mongodb/mongo-c-driver/tree/master/src/libbson

   - C++
     1. https://github.com/mongodb/mongo-cxx-driver/tree/master/src/bsoncxx
