# require 'httparty'
require 'json'
require 'aws-sdk-iot'
require 'aws-sdk-iotdataplane'
require 'slack-ruby-client'

Slack.configure do |config|
  config.token = ENV['SLACK_TOKEN']
end

def mantioned?(text)
  text.match?("<@#{ENV['SLACK_USER_ID']}>")
end

def mayday?(text)
  ['話', '求める', '相談'].any? { |keyword| text.match?(keyword) }
end

def lambda_handler(event:, context:)
  input = JSON.parse(event['body'])

  type = input['type']
  challenge = input['challenge']
  token = input['token']
  return { statusCode: 200, body: { challenge: challenge }.to_json } if type == "url_verification"

  event = input['event']
  return { statusCode: 200, body: { message: 'not message.' }.to_json } unless event['type'] == 'message'

  text = event['text']
  return { statusCode: 200, body: { message: 'not message.' }.to_json } if text.nil?

  if mantioned?(text) && mayday?(text)
    slack_client = Slack::Web::Client.new
    res = slack_client.users_info(user: event['user'])
    puts event

    iot_client = Aws::IoTDataPlane::Client.new(endpoint: ENV['IOT_CORE_ENDPOINT'])
    iot_client.publish(
      topic: '/iot-mayday/call',
      payload: {
        user_name: res.user.name,
        user_id: event['user'],
        channel: event['channel'],
        thread_ts: event['thread_ts'],
        message: text.gsub("<@#{ENV['SLACK_USER_ID']}>", '').strip
      }.to_json
    )
  end

  { statusCode: 200, body: { message: "OK", }.to_json }
end
