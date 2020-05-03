#!/usr/bin/env ruby

require 'sinatra'

get '/' do
  'ok'
end

return_status = 200

get '/vote.json' do
  status(return_status)
  File.read('vote.json')
end

post '/vote.json' do
  request.body.rewind
  File.open('vote.json', 'w') do |f|
    f.write request.body.read
  end
  'ok'
end

get '/vote.json.signature' do
  status(return_status)
  File.read('vote.json.signature')
end

post '/vote.json.signature' do
  request.body.rewind
  File.open('vote.json.signature', 'w') do |f|
    f.write request.body.read
  end
  'ok'
end

post '/status' do
  request.body.rewind
  return_status = request.body.read.to_i
  'ok'
end
